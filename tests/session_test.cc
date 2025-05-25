#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>

#include "server.h"
#include "session.h"
#include "router.h"
#include "echo_handler.h"
#include "static_handler.h"

using boost::asio::ip::tcp;
namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// SessionTest Fixture
//
// Starts a real server in a background thread using a temporary router
// configured with EchoHandler and StaticHandler for each test.
// -----------------------------------------------------------------------------
class SessionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Bind to an ephemeral port
    acceptor_.open(tcp::v4());
    acceptor_.bind({tcp::v4(), 0});
    port_ = acceptor_.local_endpoint().port();
    acceptor_.close();  // server will reopen internally

    // Build router with dynamic factories
    router_ = std::make_shared<Router>();

    // Echo on "/"
    router_->add_route(
      "/",
      [](const std::string& loc,
         const std::unordered_map<std::string,std::string>& /*p*/) {
        return HandlerRegistry::CreateHandler(EchoHandler::kName, loc, {});
      },
      {}
    );

    // Prepare static test directory
    temp_dir_ = fs::temp_directory_path() / "static_test";
    fs::create_directories(temp_dir_);

    // Static on "/static_test"
    router_->add_route(
      "/static_test",
      [](const std::string& loc,
         const std::unordered_map<std::string,std::string>& p) {
        return HandlerRegistry::CreateHandler(StaticHandler::kName, loc, p);
      },
      {{"root", temp_dir_.string()}}
    );

    // Create sample files
    create_test_file("test.txt", "this is a test");
    create_test_file("test.html", "<!doctype html><html><head><title>x</title></head><body></body></html>");

    // Launch the server in a separate thread
    server_ = std::make_unique<server>(io_service_, port_, *router_, session::MakeSession);
    io_thread_ = std::thread([this]{ io_service_.run(); });
  }

  void TearDown() override {
    io_service_.stop();
    if (io_thread_.joinable()) io_thread_.join();
  }

  // Helper to write a file into temp_dir_
  void create_test_file(const std::string& name, const std::string& content) {
    std::ofstream((temp_dir_ / name).c_str()) << content;
  }

  // Send a raw HTTP request and return the connected socket
  tcp::socket SendRequest(const std::string& req) {
    tcp::socket sock(io_service_);
    sock.connect({tcp::v4(), port_});
    boost::asio::write(sock, boost::asio::buffer(req));
    return sock;
  }

  // Read until EOF (server closes connection) and return response as string
  std::string ReadResponse(tcp::socket& sock, boost::asio::streambuf& buf, boost::system::error_code& ec) {
    boost::asio::read(sock, buf, ec);
    return {buffers_begin(buf.data()), buffers_end(buf.data())};
  }

  boost::asio::io_service io_service_;
  tcp::acceptor acceptor_{io_service_};
  unsigned short port_;
  std::unique_ptr<server> server_;
  std::thread io_thread_;
  std::shared_ptr<Router> router_;
  fs::path temp_dir_;
};

// -----------------------------------------------------------------------------
// SocketAccessor
//
// Verify that a fresh session has a closed socket before start().
// -----------------------------------------------------------------------------
TEST_F(SessionTest, SocketAccessor) {
  std::shared_ptr<session> s = session::MakeSession(io_service_, *router_);
  EXPECT_FALSE(s->socket().is_open());
}

// -----------------------------------------------------------------------------
// LargeRequest
//
// Ensure long URLs (>1KB) are echoed correctly across multiple reads.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeRequest) {
  std::string long_path(1500, 'a');
  std::string req = "GET /" + long_path + " HTTP/1.1\r\nHost: l\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// -----------------------------------------------------------------------------
// AsyncReadError
//
// If the client closes mid-read, the server should silently drop the session.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, AsyncReadError) {
  std::string req = "GET / HTTP/1.1\r\nHost: l\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  sock.close();  // force error on server side
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  EXPECT_EQ("", resp);
}

// -----------------------------------------------------------------------------
// IncompleteRequestNoEcho
//
// A request without a blank line terminator should not produce any reply.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, IncompleteRequestNoEcho) {
  std::string req = "GET /noend HTTP/1.1\r\nHost: l\r\n";  // missing CRLF
  tcp::socket sock = SendRequest(req);
  
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  bool got_response = false;

  sock.async_read_some(buf.prepare(64), [&](auto err, std::size_t n){
    if (!err && n > 0) { buf.commit(n); got_response = true; }
  });
  io_service_.run_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(got_response);
}

// -----------------------------------------------------------------------------
// InvalidRequest
//
// Malformed HTTP should immediately return 400 Bad Request.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, InvalidRequest) {
  std::string req = "GET /\r\n\r\n";  // no valid start-line
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "Bad Request");
}

// -----------------------------------------------------------------------------
// TXTRequest & HTMLRequest
//
// Verify StaticHandler serves .txt and .html files with correct body.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, TXTRequest) {
  std::string req = "GET /static_test/test.txt HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf; boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  auto body_pos = resp.find("\r\n\r\n"); ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(resp.substr(body_pos + 4), "this is a test");
}

TEST_F(SessionTest, HTMLRequest) {
  std::string req = "GET /static_test/test.html HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf; boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  auto body_pos = resp.find("\r\n\r\n"); ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(resp.substr(body_pos + 4),
            "<!doctype html><html><head><title>x</title></head><body></body></html>");
}

// -----------------------------------------------------------------------------
// MissingFile & InvalidStaticRequest
//
// 404 when file not found; malformed path returns 400.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, MissingFile) {
  std::string req = "GET /static_test/missing.txt HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf; boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  auto body_pos = resp.find("\r\n\r\n"); ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(resp.substr(body_pos + 4), "404 Error: File not found");
}

TEST_F(SessionTest, InvalidStaticRequest) {
  std::string req = "GET /static_test/test.txt\r\n\r\n";  // missing HTTP version
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf; boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  auto body_pos = resp.find("\r\n\r\n"); ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(resp.substr(body_pos + 4), "Bad Request");
}

// -----------------------------------------------------------------------------
// LargeChunkedWriteRequest
//
// Stress test: send a huge request in chunks, ensure full echo.
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeChunkedWriteRequest) {
  const size_t filler_size = 10 * 1024;  // 10 KB
  std::string random_filler; random_filler.reserve(filler_size);
  for (size_t i = 0; i < filler_size; ++i)
    random_filler.push_back(static_cast<char>((rand() % 94) + 33));

  std::string request_start =
      "GET /big HTTP/1.1\r\nHost: test\r\nX-Filler: ";
  std::string request_full = request_start + random_filler + "\r\n\r\n";

  tcp::socket sock(io_service_);
  sock.connect({tcp::v4(), port_});
  const size_t chunk_size = 300;
  for (size_t offset = 0; offset < request_full.size(); offset += chunk_size) {
    size_t len = std::min(chunk_size, request_full.size() - offset);
    boost::asio::write(sock,
      boost::asio::buffer(request_full.data() + offset, len));
  }

  boost::asio::streambuf resp_buf; boost::system::error_code ec;
  boost::asio::read(sock, resp_buf, ec);
  std::string resp_str((std::istreambuf_iterator<char>(&resp_buf)),
                        std::istreambuf_iterator<char>());

  // Check 200 OK
  EXPECT_NE(resp_str.find("HTTP/1.1 200 OK"), std::string::npos);

  // Verify echoed body matches original
  auto body_pos = resp_str.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp_str.substr(body_pos + 4);
  EXPECT_EQ(body.size(), request_full.size());
  EXPECT_EQ(body, request_full);
}

// -----------------------------------------------------------------------------
// LargeBodyRequest
//
// Stress test: send a request with a huge body, ensure that whole body is received
// and echoed back
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeBodyRequest) {
  const size_t filler_size = 512;  //  Half size of session buffer
  std::string random_filler; random_filler.reserve(filler_size);
  for (size_t i = 0; i < filler_size; ++i)
    random_filler.push_back(static_cast<char>((rand() % 94) + 33));

  const size_t body_size = 1024;  // size of session buffer
  std::string random_body; random_body.reserve(body_size);
  for (size_t i = 0; i < body_size; ++i)
    random_body.push_back(static_cast<char>((rand() % 94) + 33));

  // Session reads data in chunks of 1024 bytes, so it will read the first 1024 bytes
  // of this request, which includes \r\n\r\n. The body following this will be split up
  // due to its size of 1024 bytes.
  std::string request_start =
      "GET / HTTP/1.1\r\nContent-Length: " + 
      std::to_string(body_size) +
      "\r\nX-Filler: ";
  std::string request_full = request_start + random_filler + "\r\n\r\n" + random_body;

  tcp::socket sock = SendRequest(request_full);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  // Verify echoed request matches original
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body.size(), request_full.size());
  EXPECT_EQ(body, request_full);
}

// -----------------------------------------------------------------------------
// LargeBodyRequestNoLength
//
// Stress test: send a request with a huge body but no Content-Length, body
// should not be echoed back
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeBodyRequestNoLength) {
  const size_t filler_size = 512;  //  Half size of session buffer
  std::string random_filler; random_filler.reserve(filler_size);
  for (size_t i = 0; i < filler_size; ++i)
    random_filler.push_back(static_cast<char>((rand() % 94) + 33));

  const size_t body_size = 1024;  // size of session buffer
  std::string random_body; random_body.reserve(body_size);
  for (size_t i = 0; i < body_size; ++i)
    random_body.push_back(static_cast<char>((rand() % 94) + 33));

  // Session reads data in chunks of 1024 bytes, so it will read the first 1024 bytes
  // of this request, which includes \r\n\r\n. The body following this will be split up
  // due to its size of 1024 bytes.
  std::string request_header =
      "GET / HTTP/1.1\r\nX-Filler: " + random_filler + "\r\n\r\n";
  std::string request_full = request_header + random_body;

  tcp::socket sock = SendRequest(request_full);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  // Verify echoed request does not match original
  // Anything after \r\n\r\n shouldn't be sent back
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body.size(), request_header.size());
  EXPECT_EQ(body, request_header);
}

// -----------------------------------------------------------------------------
// LargeBodyRequestSmallerLength
//
// Stress test: send a request with a huge body but Content-Length field less than
// actual body size, ensure that only part of body comes back
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeBodyRequestSmallerLength) {
  const size_t filler_size = 512;  //  Half size of session buffer
  std::string random_filler; random_filler.reserve(filler_size);
  for (size_t i = 0; i < filler_size; ++i)
    random_filler.push_back(static_cast<char>((rand() % 94) + 33));

  const size_t body_size = 1024;  // size of session buffer
  std::string random_body; random_body.reserve(body_size);
  for (size_t i = 0; i < body_size; ++i)
    random_body.push_back(static_cast<char>((rand() % 94) + 33));

  // Content-Length set to less than actual size
  const size_t diff = 10;
  std::string request_start =
      "GET / HTTP/1.1\r\nContent-Length: " + 
      std::to_string(body_size - diff) +
      "\r\nX-Filler: ";
  std::string request_full = request_start + random_filler + "\r\n\r\n" + random_body;

  tcp::socket sock = SendRequest(request_full);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  // Verify echoed request matches part of original up to Content-Length
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body.size(), request_full.size() - diff);
  EXPECT_EQ(body, request_full.substr(0, request_full.size() - diff));
}

// -----------------------------------------------------------------------------
// LargeBodyRequestLargerLength
//
// Stress test: send a request with a huge body but Content-Length field more than
// actual body size, check that server times out
// -----------------------------------------------------------------------------
TEST_F(SessionTest, LargeBodyRequestLargerLength) {
  const size_t filler_size = 512;  //  Half size of session buffer
  std::string random_filler; random_filler.reserve(filler_size);
  for (size_t i = 0; i < filler_size; ++i)
    random_filler.push_back(static_cast<char>((rand() % 94) + 33));

  const size_t body_size = 1024;  // size of session buffer
  std::string random_body; random_body.reserve(body_size);
  for (size_t i = 0; i < body_size; ++i)
    random_body.push_back(static_cast<char>((rand() % 94) + 33));

  // Content-Length set to more than actual size
  const size_t diff = 10;
  std::string request_start =
      "GET / HTTP/1.1\r\nContent-Length: " + 
      std::to_string(body_size + diff) +
      "\r\nX-Filler: ";
  std::string request_full = request_start + random_filler + "\r\n\r\n" + random_body;

  tcp::socket sock = SendRequest(request_full);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  bool got_response = false;

  sock.async_read_some(buf.prepare(64), [&](auto err, std::size_t n){
    if (!err && n > 0) { buf.commit(n); got_response = true; }
  });
  // Wait for 2 seconds (session times out with severity level warning after 5)
  io_service_.run_for(std::chrono::seconds(2));
  EXPECT_FALSE(got_response);
}
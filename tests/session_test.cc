#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>

#include "server.h"
#include "session.h"

using boost::asio::ip::tcp;

// ----------  Fixture that starts the server on SetUp()  ---------------
class SessionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 0 asks the OS to pick any free port.
    acceptor_.open(tcp::v4());
    acceptor_.bind({tcp::v4(), 0});
    port_ = acceptor_.local_endpoint().port();
    acceptor_.close();         // we'll reopen inside server

    // start server
    server_ = std::make_unique<server>(io_service_, port_, session::MakeSession);
    io_thread_ = std::thread([this]{ io_service_.run(); });
  }

  void TearDown() override {
    io_service_.stop();
    if (io_thread_.joinable()) io_thread_.join();
  }

  // helper to send a request
  tcp::socket SendRequest(const std::string& req) {
    tcp::socket sock(io_service_);
    sock.connect({tcp::v4(), port_});
    boost::asio::write(sock, boost::asio::buffer(req));
    return sock;
  }

  // helper to read a response
  std::string ReadResponse(tcp::socket& sock, boost::asio::streambuf& buf, boost::system::error_code& ec) {
    // read until EOF (server closes connection)
    boost::asio::read(sock, buf, ec);
    return {buffers_begin(buf.data()), buffers_end(buf.data())};
  }

  boost::asio::io_service io_service_;
  tcp::acceptor acceptor_{io_service_};
  unsigned short port_;
  std::unique_ptr<server> server_;
  std::thread io_thread_;
};

// --------------------------------Session unit tests----------------------------
TEST_F(SessionTest, SocketAccessor) {
  session* s = session::MakeSession(io_service_);
  EXPECT_FALSE(s->socket().is_open()); // socket created but not yet open
  delete s;
}

// ----------------  Server-session tests  ------------

// 1) Large request ( > internal 1 kB buffer ) that calls handle_read multiple times
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

// 2) Error occurs during async_read do to socket closure
TEST_F(SessionTest, AsyncReadError) {
  std::string req = "GET / HTTP/1.1\r\nHost: l\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  sock.close(); // Close while its reading the request
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  EXPECT_EQ("", resp);
}

// 3) Request missing terminator should NOT get a response (timeout)
TEST_F(SessionTest, IncompleteRequestNoEcho) {
  std::string req = "GET /noend HTTP/1.1\r\nHost: l\r\n"; // no blank line
  tcp::socket sock = SendRequest(req);
  
  boost::asio::streambuf buf;
  boost::system::error_code ec;

  // wait briefly for data that shouldn't come
  bool got_response = false;
  sock.async_read_some(buf.prepare(64), [&](const boost::system::error_code& ec, std::size_t bytes_transferred){
    if (!ec && bytes_transferred > 0) {
      buf.commit(bytes_transferred);
      got_response = true;
    }
  });
  io_service_.run_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(got_response);  // nothing was echoed
}

// FINAL LEVEL!
// A "stress" test that:
//   1) Creates a GET request with a huge header containing random data
//      (well beyond the 1 KB chunk size).
//   2) Sends the request in several partial writes, to force the server
//      to handle multiple async_read iterations.
//   3) Verifies the full original request is echoed back in the response body.
TEST_F(SessionTest, LargeChunkedWriteRequest) {
  // 1) Generate a random filler string to simulate "fr fr" data in a header.
  //    We'll embed this in a header line, plus a normal GET start-line.
  const size_t filler_size = 10 * 1024;  // 10 KB
  std::string random_filler;
  random_filler.reserve(filler_size);

  // Fill with random printable ASCII from [32..126].
  // (You can seed with a fixed value for reproducibility.)
  for (size_t i = 0; i < filler_size; ++i) {
    char c = static_cast<char>((rand() % 95) + 32);
    random_filler.push_back(c);
  }

  // Build the full request in a single string.
  // We'll *write* it in multiple chunks below, but conceptually it's one request.
  std::string request_start =
      "GET /big HTTP/1.1\r\n"
      "Host: test\r\n"
      "X-Filler: ";  // We'll put our big random data here

  // “X-Filler: <random string>\r\n\r\n”
  std::string request_full =
      request_start + random_filler + "\r\n\r\n";

  // 2) Connect to the server, then *intentionally* send partial chunks of
  //    the request in multiple writes to test repeated reads on the server.
  tcp::socket sock(io_service_);
  sock.connect({tcp::v4(), port_});

  // We'll break the large request into ~300‑byte chunks.
  // (You can tweak chunk_size to test different read patterns.)
  const size_t chunk_size = 300;
  for (size_t offset = 0; offset < request_full.size(); offset += chunk_size) {
    size_t len = std::min(chunk_size, request_full.size() - offset);
    // Write this chunk to the socket
    boost::asio::write(sock,
                       boost::asio::buffer(request_full.data() + offset, len));
    // Optional small sleep to simulate real-world network latency:
    // std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  // 3) Read the server's response until EOF. Then validate it.
  boost::asio::streambuf resp_buf;
  boost::system::error_code ec;
  boost::asio::read(sock, resp_buf, ec);  // reading until connection close

  std::string resp_str((std::istreambuf_iterator<char>(&resp_buf)),
                        std::istreambuf_iterator<char>());

  // Check: 200 OK status line and we got everything echoed.
  EXPECT_NE(resp_str.find("HTTP/1.1 200 OK"), std::string::npos)
      << "Expected 200 OK in the response";

  // Find the body (the echoed request) in the response.
  auto body_pos = resp_str.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos)
      << "Expected blank line indicating start of body";
  std::string body = resp_str.substr(body_pos + 4);

  EXPECT_EQ(body.size(), request_full.size())
      << "The echoed body should be exactly the size of the original request";
  EXPECT_EQ(body, request_full)
      << "The server's echo did not match the original request content";
}

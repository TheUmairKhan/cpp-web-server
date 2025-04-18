// tests/echo_server_test.cc
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>

#include "server.h"

using boost::asio::ip::tcp;

// ----------  Helper to write a tiny config file on‑the‑fly  ----------
static std::string MakeTempConfig(unsigned short port) {
  const std::string path = "/tmp/echotest_" + std::to_string(port) + ".conf";
  std::ofstream f(path);
  f << "port " << port << ";\n";
  f.close();
  return path;
}

// ----------  Fixture that starts the server on SetUp()  ---------------
class EchoServerFixture : public ::testing::Test {
protected:
  void SetUp() override {
    // 0 asks the OS to pick any free port.
    acceptor_.open(tcp::v4());
    acceptor_.bind({tcp::v4(), 0});
    port_ = acceptor_.local_endpoint().port();
    acceptor_.close();         // we'll reopen inside server

    cfg_path_ = MakeTempConfig(port_);

    // start server
    server_ = std::make_unique<server>(io_, port_);
    io_thread_ = std::thread([this]{ io_.run(); });
  }

  void TearDown() override {
    io_.stop();
    if (io_thread_.joinable()) io_thread_.join();
    std::remove(cfg_path_.c_str());
  }

  // helper to send a request and read full response
  std::string RoundTrip(const std::string& req) {
    tcp::socket sock(io_);
    sock.connect({tcp::v4(), port_});
    boost::asio::write(sock, boost::asio::buffer(req));
    // read until EOF (server closes connection)
    boost::asio::streambuf buf;
    boost::system::error_code ec;
    boost::asio::read(sock, buf, ec);
    return {buffers_begin(buf.data()), buffers_end(buf.data())};
  }

  boost::asio::io_service io_;
  tcp::acceptor acceptor_{io_};
  unsigned short port_;
  std::string cfg_path_;
  std::unique_ptr<server> server_;
  std::thread io_thread_;
};

// ----------------  Echo‑server integration tests  ------------

// 1) Simple GET with CRLF terminator
TEST_F(EchoServerFixture, BasicEcho) {
  std::string req =
      "GET /foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  std::string resp = RoundTrip(req);

  // Status line & header checks
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(resp.find("Content-Type: text/plain"), std::string::npos);

  // Body should exactly equal original request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 2) Large request ( > internal 1 kB buffer )
TEST_F(EchoServerFixture, LargeRequest) {
  std::string long_path(1500, 'a');
  std::string req = "GET /" + long_path + " HTTP/1.1\r\nHost: l\r\n\r\n";
  std::string resp = RoundTrip(req);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 3) Request missing terminator should NOT get a response (timeout)
TEST_F(EchoServerFixture, IncompleteRequestNoEcho) {
  tcp::socket sock(io_);
  sock.connect({tcp::v4(), port_});
  std::string half = "GET /noend HTTP/1.1\r\nHost: l\r\n"; // no blank line
  boost::asio::write(sock, boost::asio::buffer(half));

  boost::asio::streambuf buf;
  boost::system::error_code ec;
  // wait briefly for data that shouldn't come
  sock.async_read_some(buf.prepare(64), [&](auto, auto){});
  io_.run_one_for(std::chrono::milliseconds(50));

  EXPECT_EQ(buf.size(), 0u);  // nothing was echoed
}

// 4) Malformed start‑line should return 400
TEST_F(EchoServerFixture, MalformedStartLine) {
  std::string bad = "G?T /oops HTTP/1.1\r\nHost: x\r\n\r\n";
  std::string resp = RoundTrip(bad);

  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);
  // Body should be empty
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 5) NetCat Example, where the request uses only LF line breaks instead of CRLF
TEST_F(EchoServerFixture, NetCatTest) {
  std::string req =
      "GET /foo HTTP/1.1\nHost: localhost\n\n";
  std::string resp = RoundTrip(req);

  // Status line & header checks
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(resp.find("Content-Type: text/plain"), std::string::npos);

  // Body should exactly equal original request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 6) A POST request should return 400, since we only handle GET at the moment
TEST_F(EchoServerFixture, PostRequestReturns400) {
  std::string req =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n"
      "Body data that should be ignored";
  std::string resp = RoundTrip(req);

  // Verify the status line.
  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);

  // Body should be empty since we return 400.
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 7) A HEAD request should return 400, since we only handle GET at the moment
TEST_F(EchoServerFixture, HeadRequestReturns400) {
  std::string req =
      "HEAD / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";
  std::string resp = RoundTrip(req);

  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 8) A valid GET with additional body content. Should be echo'd back by the server.
TEST_F(EchoServerFixture, GETWithRequestBody) {
  // Notice we have a body even though GET typically doesn't carry one.
  // The server's echo logic will include it in the response body.
  std::string req =
      "GET /carry_body HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "Hello";  // This is the "body" that comes right after blank line
  std::string resp = RoundTrip(req);

  // The status should be 200 OK, and the body should match the entire request
  // (headers + the "Hello" part).
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 9) Request with only LF line breaks instead of CRLF.
TEST_F(EchoServerFixture, OnlyLFLineBreaks) {
  // We'll just separate lines with '\n'. This should be recognized
  // by the server's request_complete() check that looks for "\n\n".
  std::string req =
      "GET /lf_only HTTP/1.1\n"
      "Host: x\n"
      "\n";  // End of headers with a single blank line
  std::string resp = RoundTrip(req);

  // Confirm 200 OK. Then ensure the echoed body is exactly what we sent.
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// FINAL LEVEL!
// A "stress" integration test that:
//   1) Creates a GET request with a huge header containing random data
//      (well beyond the 1 KB chunk size).
//   2) Sends the request in several partial writes, to force the server
//      to handle multiple async_read iterations.
//   3) Verifies the full original request is echoed back in the response body.
TEST_F(EchoServerFixture, LargeChunkedWriteRequest) {
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
  tcp::socket sock(io_);
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
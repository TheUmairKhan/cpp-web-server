// tests/echo_server_test.cc
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>

#include "server.h"
#include "config_parser.h"

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

// ----------------  Config‑parser unit tests  -----------------
TEST(ConfigParserTest, ValidConfig) {
  NginxConfigParser p; NginxConfig cfg;
  std::stringstream ss("port 1234;");
  ASSERT_TRUE(p.Parse(&ss, &cfg));          //  ← use stream
  unsigned short port = 0;
  for (auto& s : cfg.statements_)
    if (s->tokens_[0] == "port") port = std::stoi(s->tokens_[1]);
  EXPECT_EQ(port, 1234);
}

TEST(ConfigParserTest, InvalidConfig) {
  NginxConfigParser p; NginxConfig cfg;
  std::stringstream ss("port bad_no_semicolon");
  EXPECT_FALSE(p.Parse(&ss, &cfg));         //  ← use stream
}

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

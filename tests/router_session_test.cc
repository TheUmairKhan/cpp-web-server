#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>

#include "server.h"
#include "router_session.h"
#include "router.h"
#include "echo_handler.h"
#include "static_handler.h"

using boost::asio::ip::tcp;

// ----------  Fixture that starts the server on SetUp()  ---------------
class RouterSessionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 0 asks the OS to pick any free port.
    acceptor_.open(tcp::v4());
    acceptor_.bind({tcp::v4(), 0});
    port_ = acceptor_.local_endpoint().port();
    acceptor_.close();         // we'll reopen inside server

    router_ = std::make_shared<Router>();
    router_->add_route("/", std::make_unique<EchoHandler>());
    StaticHandler::configure("/static", "/usr/src/projects/lebron2016x4/tests/test_directory");
    router_->add_route("/static", std::make_unique<StaticHandler>());
    
    // start server
    server_ = std::make_unique<server>(io_service_, port_, [router=router_](boost::asio::io_service& io)
                 { return RouterSession::Make(io, *router); });
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
  std::shared_ptr<Router> router_;
};

// ----------------  Server-session tests  ------------

// Requests test.txt file 
TEST_F(RouterSessionTest, TXTRequest) {
  std::string req = "GET /static/test.txt HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "this is a test");
}

// Requests test.html file 
TEST_F(RouterSessionTest, HTMLRequest) {
  std::string req = "GET /static/test.html HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "<!doctype html><html><head><title>x</title></head><body></body></html>");
}

// Error occurs during async_read due to socket closure
TEST_F(RouterSessionTest, AsyncReadError) {
  std::string req = "GET / HTTP/1.1\r\nHost: \r\n\r\n";
  tcp::socket sock = SendRequest(req);
  sock.close(); // Close while its reading the request
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);
  EXPECT_EQ("", resp);
}

// Request missing terminator should NOT get a response (timeout)
TEST_F(RouterSessionTest, IncompleteRequestNoEcho) {
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

// Missing file should return 404 error
TEST_F(RouterSessionTest, MissingFile) {
  std::string req = "GET /static/missing.txt HTTP/1.1\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "404 Error: File not found");
}

// Bad request should return 400 error
TEST_F(RouterSessionTest, InvalidRequest) {
  std::string req = "GET /static/test.txt\r\n\r\n";
  tcp::socket sock = SendRequest(req);
  boost::asio::streambuf buf;
  boost::system::error_code ec;
  std::string resp = ReadResponse(sock, buf, ec);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "Bad Request");
}

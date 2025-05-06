#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <fstream>
#include <gmock/gmock.h>

#include "server.h"

using boost::asio::ip::tcp;

// Mock session class for testing server
class MockSession : public session {
  public:
    static MockSession* MakeMockSession(boost::asio::io_service& io_service, Router& r) {
      return new MockSession(io_service, r);
    }

    tcp::socket& socket() override {
      return mock_socket_;
    }

    void start() override {
      throw std::runtime_error("Mock session started");
    }

  private:
    MockSession(boost::asio::io_service& io_service, Router& r)
    : session(io_service, r), mock_socket_(io_service) {}
    
    tcp::socket mock_socket_;
};

// --------------------------Helper Functions-----------------------------
// Helper to get unused port number
unsigned short GetOpenPort(boost::asio::io_service& io_service) {
  tcp::endpoint endpoint(tcp::v4(), 0); // OS assigns available port
  tcp::acceptor acceptor(io_service, endpoint);
  return acceptor.local_endpoint().port();
}

// ----------  Fixture that starts the server on SetUp()  ---------------
class ServerTest : public ::testing::Test {
protected:
  void TearDown() override {
    io_service.stop();
  }

  boost::asio::io_service io_service;
  unsigned short port;
};

// -------------------server unit tests-------------------

TEST_F(ServerTest, ConnectionAcceptedAndSessionStarts) {
  // Start server
  port = GetOpenPort(io_service);
  // server s(io_service, port, MockSession::MakeMockSession);
  Router router;
  server s(io_service, port, router, MockSession::MakeMockSession);

  // Server should start mock session which will throw an error
  EXPECT_THROW({
    // Simulate client connection to server
    tcp::socket socket(io_service);
    tcp::resolver resolver(io_service);
    tcp::resolver::query query("127.0.0.1", std::to_string(port));
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::connect(socket, endpoint_iterator);
    io_service.run();
    }, std::runtime_error);
}

TEST_F(ServerTest, ServerPortInUse) {
  // Create acceptor that is using a port
  port = GetOpenPort(io_service);
  tcp::endpoint endpoint(tcp::v4(), port);
  tcp::acceptor occupied_acceptor(io_service);
  occupied_acceptor.open(endpoint.protocol());
  //occupied_acceptor.set_option(tcp::acceptor::reuse_address(true));
  occupied_acceptor.bind(endpoint);
  occupied_acceptor.listen();

  Router router;
  // Server should not be created and will throw error
  EXPECT_THROW(server s(io_service, port, router, MockSession::MakeMockSession), boost::system::system_error);
  occupied_acceptor.close();
}

#include "server.h"
#include "logger.h"
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

server::server(boost::asio::io_service& io_service, 
               short port, 
               Router& router,
               SessionFactory session_factory)
  : io_service_(io_service),
    acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
    router_(router),
    session_factory_(session_factory) {
  Logger::log_info("Server listening on port " + std::to_string(port));
  start_accept();
}

void server::start_accept() {
  session* new_session = session_factory_(io_service_, router_);
  acceptor_.async_accept(
    new_session->socket(),
    boost::bind(&server::handle_accept, this, new_session,
                boost::asio::placeholders::error));
}

void server::handle_accept(session* new_session,
                           const boost::system::error_code& error) {
  if (!error) {
    Logger::log_info("Accepted connection from " + new_session->socket().remote_endpoint().address().to_string());
    new_session->start();
  } else {
    Logger::log_error("Accept error: " + error.message());
    delete new_session;
  }

  // Continue accepting connections
  start_accept();
}
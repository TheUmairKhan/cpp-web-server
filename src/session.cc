#include "session.h"
#include "logger.h"
#include "request.h"
#include "echo_handler.h"
#include "static_handler.h"

#include <boost/bind.hpp>
#include <string>

using boost::asio::ip::tcp;

session* session::MakeSession(boost::asio::io_service& io, Router& r) {
    return new session(io, r);
}

session::session(boost::asio::io_service& io_service, Router& r)
  : socket_(io_service), router_(r) {}

// Return the underlying socket so the acceptor can bind to it.
tcp::socket& session::socket() {
  return socket_;
}

bool session::request_complete(const std::string& in_buf) {
  return ::request_complete(in_buf);  // From request.h
}

void session::start() {
  // Kick off the first asynchronous read.
  auto ip = Logger::get_client_ip(socket_);
  Logger::log_connection(ip);
  socket_.async_read_some(
      boost::asio::buffer(chunk_, max_length),
      boost::bind(&session::handle_read, this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error,
                                std::size_t bytes_transferred)
{
    if (error) { delete this; return; }

    in_buf_.append(chunk_, bytes_transferred);

    if (!request_complete(in_buf_)) {
        socket_.async_read_some(
            boost::asio::buffer(chunk_, max_length),
            [this](const boost::system::error_code& err, std::size_t n) {
                handle_read(err, n);
            });
        return;
    }

    // Parse
    Request  request(in_buf_);

    // Add logger
    std::string client_ip = Logger::get_client_ip(socket_);

    //Early 400 on malformed syntax
    if (!request.is_valid()) {
        Response bad_response(request.get_version(), 400, "text/plain", /*content len=*/11, "close", "Bad Request"
        );    
        Logger::log_request(client_ip, request.get_method(), request.get_url(), 400);
        boost::asio::async_write(
        socket_,
        boost::asio::buffer(bad_response.to_string()),
        boost::bind(&session::handle_write, this,
                    boost::asio::placeholders::error));
        return;
    }

    Response response = router_.handle_request(request);

    // Log actual status code (200, 404, etc.)
    int code = response.get_status_code();
    Logger::log_request(
        client_ip,
        request.get_method(),
        request.get_url(),
        code
    );

    out_buf_ = response.to_string();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(out_buf_),
        [this](const boost::system::error_code& err, std::size_t) {
            handle_write(err);
        });
}

void session::handle_write(const boost::system::error_code& error) {
  // We’re done with this connection—close it either way.
  delete this;
}
#include "session.h"
#include "echo_handler.h"

#include <boost/bind.hpp>
#include <string>

using boost::asio::ip::tcp;

session* session::MakeSession(boost::asio::io_service& io_service) {
  return new session(io_service);
}

session::session(boost::asio::io_service& io_service)
  : socket_(io_service) {}

// Return the underlying socket so the acceptor can bind to it.
tcp::socket& session::socket() {
  return socket_;
}

void session::start() {
  // Kick off the first asynchronous read.
  socket_.async_read_some(
      boost::asio::buffer(chunk_, max_length),
      boost::bind(&session::handle_read, this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error,
                          std::size_t bytes_transferred) {
  if (error) {
    delete this;
    return;
  }

  // Append what we just read to the growing request buffer.
  in_buf_.append(chunk_, bytes_transferred);

  // Keep reading until we detect the end of the HTTP headers.
  if (!request_complete()) {
    socket_.async_read_some(
        boost::asio::buffer(chunk_, max_length),
        boost::bind(&session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return;
  }

  // Build the HTTP response into out_buf_.
  std::unique_ptr<RequestHandler> handler = std::make_unique<EchoHandler>();
  handler->handle_request(in_buf_, out_buf_);

  // Write the entire response back to the client.
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(out_buf_),
      boost::bind(&session::handle_write, this,
                  boost::asio::placeholders::error));
}

void session::handle_write(const boost::system::error_code& error) {
  // We’re done with this connection—close it either way.
  delete this;
}

bool session::request_complete() const {
  // Simple heuristic: headers end with a blank line (\r\n\r\n).
  // Added || for \n\n termination for the netcat terminal, since their newline doesn't produce \r\n but \n instead.
  return in_buf_.find("\r\n\r\n") != std::string::npos || in_buf_.find("\n\n") != std::string::npos;
}
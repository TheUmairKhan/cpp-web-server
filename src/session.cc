#include "session.h"
#include "request.h"
#include "echo_handler.h"
#include "static_handler.h"

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
  if (!request_complete(in_buf_)) {
    socket_.async_read_some(
        boost::asio::buffer(chunk_, max_length),
        boost::bind(&session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    return;
  }

  // Create Request object
  Request request(in_buf_);

  // Decide what handler to use
  // For now, everything defaults to echo except for /static
  std::unique_ptr<RequestHandler> handler;
  if (request.get_url().rfind("/static", 0) == 0) handler = std::make_unique<StaticHandler>();
  else handler = std::make_unique<EchoHandler>();
  // Build the HTTP response
  Response response = handler->handle_request(request);


  // Write the entire response back to the client.
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(response.to_string()),
      boost::bind(&session::handle_write, this,
                  boost::asio::placeholders::error));
}

void session::handle_write(const boost::system::error_code& error) {
  // We’re done with this connection—close it either way.
  delete this;
}

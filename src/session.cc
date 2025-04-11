#include "session.h"

#include <boost/bind.hpp>
#include <string>

using boost::asio::ip::tcp;

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
  make_response();

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
  return in_buf_.find("\r\n\r\n") != std::string::npos;
}

void session::make_response() {
  // Grab the first line (up to CR/LF or LF) to inspect the method
  auto eol = in_buf_.find("\r\n");
  if (eol == std::string::npos) eol = in_buf_.find('\n');
  std::string first_line = in_buf_.substr(0, eol);

  bool is_get = first_line.rfind("GET ", 0) == 0;   // starts with "GET "

  out_buf_.clear();

  if (!is_get) {   // ---------- 400 path ----------
    out_buf_  = "HTTP/1.1 400 Bad Request\r\n";
    out_buf_ += "Content-Length: 0\r\n";
    out_buf_ += "Connection: close\r\n\r\n";
    return;
  }

  // ---------- normal 200 echo path ----------
  const std::string& body = in_buf_;
  out_buf_  = "HTTP/1.1 200 OK\r\n";
  out_buf_ += "Content-Type: text/plain\r\n";
  out_buf_ += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  out_buf_ += "Connection: close\r\n\r\n";
  out_buf_ += body;
}

#include "session.h"
#include "logger.h"
#include "request.h"
#include "echo_handler.h"
#include "static_handler.h"

#include <boost/bind.hpp>
#include <string>
#include <sstream>

using boost::asio::ip::tcp;

std::shared_ptr<session> session::MakeSession(boost::asio::io_service& io, Router& r) {
    return std::shared_ptr<session>(new session(io, r));
}

session::session(boost::asio::io_service& io_service, Router& r)
  : socket_(io_service), router_(r),
    timer_(io_service.get_executor().context()){}

// Return the underlying socket so the acceptor can bind to it.
tcp::socket& session::socket() {
  return socket_;
}

void session::start() {
  auto self = shared_from_this();
  // Kick off the first asynchronous read.
  auto ip = Logger::get_client_ip(socket_);
  Logger::log_connection(ip);
  start_timer();
  socket_.async_read_some(
      boost::asio::buffer(chunk_, max_length),
      boost::bind(&session::handle_read, self,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error,
                                std::size_t bytes_transferred)
{
  auto self = shared_from_this();

  if (error) { 
    stop_timer();
    return; 
  }

  in_buf_.append(chunk_, bytes_transferred);

  if (!request_complete()) {
    start_timer(); // Restart timer after receiving
    socket_.async_read_some(
        boost::asio::buffer(chunk_, max_length),
        [self](const boost::system::error_code& err, std::size_t n) {
            self->handle_read(err, n);
        });
    return;
  }

  // Stop timer if complete request read
  stop_timer();

  // Parse
  Request  request(in_buf_);

  // Add logger
  std::string client_ip = Logger::get_client_ip(socket_);

    //Early 400 on malformed syntax
    if (!request.is_valid()) {
        Response bad_response("HTTP/1.1", 400, "text/plain", /*content len=*/11, "close", "Bad Request"
        );    
        Logger::log_request(client_ip, request.get_method(), request.get_url(), 400, bad_response.get_handler_type());
        boost::asio::async_write(
        socket_,
        boost::asio::buffer(bad_response.to_string()),
        boost::bind(&session::handle_write, self,
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
        code,
        response.get_handler_type()
    );

  out_buf_ = response.to_string();
  boost::asio::async_write(
      socket_,
      boost::asio::buffer(out_buf_),
      [self](const boost::system::error_code& err, std::size_t) {
          self->handle_write(err);
      });
}

void session::handle_write(const boost::system::error_code& error) {
  // We’re done with this connection—close it either way.
}

bool session::request_complete() {
  // Simple heuristic: headers end with a blank line (\r\n\r\n).
  // Added || for \n\n termination for the netcat terminal, since their newline doesn't produce \r\n but \n instead.
  auto header_end_pos = in_buf_.find("\r\n\r\n");
  auto body_start_pos = header_end_pos;
  if (header_end_pos != std::string::npos)
    body_start_pos = header_end_pos + 4;
  else {
    header_end_pos = in_buf_.find("\n\n");
    if (header_end_pos != std::string::npos) 
      body_start_pos = header_end_pos + 2;
    else // header not completed yet
      return false;
  }

  // After headers, there may or may not be a body
  // Expect a Content-Length header if request has a body
  auto header = in_buf_.substr(0, body_start_pos);
  std::size_t content_length = 0;
  std::istringstream stream(header);
  std::string line;
  bool content_length_found = false;
  while (std::getline(stream, line)) {
    auto content_length_pos = line.find("Content-Length:");
    if (content_length_pos != std::string::npos) {
      content_length = std::stoull(line.substr(content_length_pos + 15));
      content_length_found = true;
      break;
    }
  }
  // If Content-Length header not found, assumes that there is no body and simply returns that request is complete
  // It is possible that a body still exists, however if there is no Content-Length header the session will remove
  // anything following \r\n\r\n
  if (!content_length_found) {
    in_buf_.resize(in_buf_.size() - (in_buf_.size() - body_start_pos));
    return true;
  }

  // If the Content-Length header exists and received body is shorter than expected, need to keep receiving
  // If received body is larger than expected, stop receiving and resize to match expected length
  auto body = in_buf_.substr(body_start_pos);
  auto body_size = body.size();
  if (body_size < content_length) return false;
  else if (body_size > content_length) {
    in_buf_.resize(in_buf_.size() - (body_size - content_length));
  }
  return true;
}

void session::start_timer() {
  auto self = shared_from_this();
  // Session waits 5 seconds to read more data, times out if nothing new received
  timer_.expires_after(std::chrono::seconds(5));
  timer_.async_wait([self](const boost::system::error_code& error) {
      self->handle_timeout(error);
  });
}

void session::stop_timer() {
  boost::system::error_code ec;
  timer_.cancel(ec);
}

void session::handle_timeout(const boost::system::error_code& error) {
  if (!error) {
    Logger::log_warning("Session timed out before receiving a complete request");
    boost::system::error_code ec;
    timer_.cancel(ec);
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
  }
}
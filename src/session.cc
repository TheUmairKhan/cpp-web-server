#include "session.h"
#include <boost/bind.hpp>

using boost::asio::ip::tcp;

session::session(boost::asio::io_service& io_service)
  : socket_(io_service) {}

boost::asio::ip::tcp::socket& session::socket() {
  return socket_;
}

void session::start() {
  socket_.async_read_some(
    boost::asio::buffer(data_, max_length),
    boost::bind(&session::handle_read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
}

void session::handle_read(const boost::system::error_code& error,
                          std::size_t bytes_transferred) {
  if (!error) {
    boost::asio::async_write(
      socket_,
      boost::asio::buffer(data_, bytes_transferred),
      boost::bind(&session::handle_write, this,
                  boost::asio::placeholders::error));
  } else {
    delete this;
  }
}

void session::handle_write(const boost::system::error_code& error) {
  if (!error) {
    socket_.async_read_some(
      boost::asio::buffer(data_, max_length),
      boost::bind(&session::handle_read, this,
                  boost::asio::placeholders::error,
                  boost::asio::placeholders::bytes_transferred));
  } else {
    delete this;
  }
}
#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>

class session {
public:
  explicit session(boost::asio::io_service& io_service);

  // Accessor for the underlying socket
  boost::asio::ip::tcp::socket& socket();

  // Start asynchronous read loop
  void start();

private:
  // Handlers for read/write
  void handle_read(const boost::system::error_code& error,
                   std::size_t bytes_transferred);
  void handle_write(const boost::system::error_code& error);

  boost::asio::ip::tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};

#endif // SESSION_H
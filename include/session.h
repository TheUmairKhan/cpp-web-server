#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>

class session {
public:
  // Session Factory Method
  static session* MakeSession(boost::asio::io_service& io_service);

  // Accessor for the underlying socket
  virtual boost::asio::ip::tcp::socket& socket();

  // Start asynchronous read loop
  virtual void start();

protected:
  explicit session(boost::asio::io_service& io_service);

private:
  // Handlers for read/write
  void handle_read(const boost::system::error_code& error,
                   std::size_t bytes_transferred);
  void handle_write(const boost::system::error_code& error);

  // bool request_complete() const; // have we seen "\r\n\r\n" ?

  boost::asio::ip::tcp::socket socket_;
  
  std::string in_buf_;   // accumulates incoming requests
  std::string out_buf_;  // full HTTP response we'll send back

  // 1KB scratch buffer. Fine for echo server, but maybe reconsider sizing for future assignments
  enum { max_length = 1024 };
  char chunk_[max_length];
};

#endif // SESSION_H
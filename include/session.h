#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include "request.h"  // Add this

class session {
public:
  static session* MakeSession(boost::asio::io_service& io_service);

  virtual boost::asio::ip::tcp::socket& socket();

  virtual void start();

protected:
  explicit session(boost::asio::io_service& io_service);

  // Add this helper function
  static bool request_complete(const std::string& in_buf) {
    return ::request_complete(in_buf);  // From request.h
  }

  virtual void handle_read(const boost::system::error_code& error,
                  std::size_t bytes_transferred);
  
  virtual void handle_write(const boost::system::error_code& error);

  boost::asio::ip::tcp::socket socket_;
  
  std::string in_buf_;
  std::string out_buf_;
  
  enum { max_length = 1024 };
  char chunk_[max_length];

private:  // Add private section for non-inherited members
  // Move any non-shared logic here if needed
};

#endif // SESSION_H
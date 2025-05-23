#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include "router.h"

class session {
public:
  static session* MakeSession(boost::asio::io_service& io_service, Router& router);

  virtual boost::asio::ip::tcp::socket& socket();

  virtual void start();

protected:
  explicit session(boost::asio::io_service& io_service, Router& router);

  virtual void handle_read(const boost::system::error_code& error,
                  std::size_t bytes_transferred);
  
  virtual void handle_write(const boost::system::error_code& error);

  // Returns true if the request in in_buf_ is complete and false otherwise
  bool request_complete();

  boost::asio::ip::tcp::socket socket_;
  Router& router_;
  
  std::string in_buf_;
  std::string out_buf_;
  
  enum { max_length = 1024 };
  char chunk_[max_length];
};

#endif // SESSION_H
#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include "session.h"

using SessionFactory = std::function<session*(boost::asio::io_service&)>;

class server {
public:
  server(boost::asio::io_service& io_service, 
         short port, 
         SessionFactory session_factory);

private:
  void start_accept();
  void handle_accept(session* new_session,
                     const boost::system::error_code& error);

  boost::asio::io_service& io_service_;
  boost::asio::ip::tcp::acceptor acceptor_;
  SessionFactory session_factory_;
};

#endif // SERVER_H
//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include "config_parser.h"
#include "server.h"

int main(int argc, char* argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }
    NginxConfigParser parser;
    NginxConfig config;

    if (!parser.Parse(argv[1], &config)) {
      std::cerr << "Config parse error\n";
      return 1;
    }

    unsigned short port = 0;
    if (!config.ExtractPort(port)) {
      std::cerr << "No valid \"port <num>;\" directive found\n";
      return 1;
    }

    boost::asio::io_service io_service;
    server s(io_service, port);
    io_service.run();
  }

  catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
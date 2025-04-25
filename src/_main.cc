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
#include "logger.h"

int main(int argc, char* argv[]) {
  try {
    // Initialize logger
    Logger::init_logger();

    if (argc != 2) {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      Logger::log_error("Usage: async_tcp_echo_server <port>");
      return 1;
    }
    NginxConfigParser parser;
    NginxConfig config;

    if (!parser.Parse(argv[1], &config)) {
      std::cerr << "Config parse error\n";
      Logger::log_config_parsing(argv[1], false);
      return 1;
    }

    //Successful parse
    Logger::log_config_parsing(argv[1], true);

    unsigned short port = 0;
    if (!config.ExtractPort(port)) {
      std::cerr << "No valid \"port <num>;\" directive found\n";
      Logger::log_error("No valid \"port <num>;\" directive found in config");
      return 1;
    }

    //Server startup
    Logger::log_server_startup(port);

    //Catch Ctrl+C and servershutdown
    std::signal(SIGINT, [](int){
      Logger::log_server_shutdown();
      std::_Exit(0);
    });


    boost::asio::io_service io_service;
    server s(io_service, port, session::MakeSession);
    io_service.run();
  }

  catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    Logger::log_error(std::string("Unhandled exception: ") + e.what());
  }

  return 0;
}
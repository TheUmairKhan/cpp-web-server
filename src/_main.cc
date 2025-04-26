//
// async_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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
#include <memory>
#include <csignal>
#include "config_parser.h"
#include "server.h"
#include "session.h"
#include "logger.h"          // ← existing logging facility
#include "router.h"
#include "echo_handler.h"
#include "static_handler.h"
#include "router_session.h"

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
  try {
    /* ───────────── Logger bootstrap ───────────── */
    Logger::init_logger();

    /* ───────────── CLI / argument check ───────── */
    if (argc != 2) {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      Logger::log_error("Usage: async_tcp_echo_server <port>");
      return 1;
    }

    /* ───────────── Parse config file ──────────── */
    NginxConfigParser parser;
    NginxConfig       config;

    if (!parser.Parse(argv[1], &config)) {
      std::cerr << "Config parse error\n";
      Logger::log_config_parsing(argv[1], false);
      return 1;
    }

    // Successful Parse
    Logger::log_config_parsing(argv[1], true);

    /* ───────────── Extract port ───────────────── */
    unsigned short port = 0;
    if (!config.ExtractPort(port)) {
      std::cerr << "No valid \"port <num>;\" directive found\n";
      Logger::log_error("No valid \"port <num>;\" directive found in config");
      return 1;
    }

    /* ───────────── Extract routes ─────────────── */
    std::vector<NginxConfig::RouteConfig> routes;
    if (!config.ExtractRoutes(routes)) {
      std::cerr << "No valid routes in config\n";
      Logger::log_error("No valid routes in config");
      return 1;
    }

    /* ───────────── Build router ───────────────── */
    Router router;
    for (const auto& route : routes) {
      if (route.handler_type == "echo") {
        router.add_route(route.path, std::make_unique<EchoHandler>());
      }
      else if (route.handler_type == "static") {
        auto it = route.params.find("root");
        if (it == route.params.end()) {
          std::cerr << "Static handler missing root directory\n";
          Logger::log_error("Static handler missing root directory for path " +
                            route.path);
          return 1;
        }
        StaticHandler::configure(route.path, it->second);
        router.add_route(route.path, std::make_unique<StaticHandler>());
      }
      // (Extend here for new handler types.)
    }

    /* ───────────── Start server ───────────────── */
    Logger::log_server_startup(port);

    //Catch Ctrl+C and servershutdown
    std::signal(SIGINT, [](int){
      Logger::log_server_shutdown();
      std::_Exit(0);
    });

    // Initialize io_service
    boost::asio::io_service io_service;

    // Clean shutdown on SIGINT / SIGTERM
    boost::asio::signal_set signals(io_service, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto) {
      Logger::log_server_shutdown();
      io_service.stop();
    });

    server srv(io_service, port,
               [&router](boost::asio::io_service& io)
                 { return RouterSession::Make(io, router); });

    std::cout << "Server running on port " << port << "\n";
    io_service.run();
  }
  catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    Logger::log_error(std::string("Unhandled exception: ") + e.what());
  }

  return 0;
}
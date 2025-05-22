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
#include <thread>
#include "config_parser.h"
#include "server.h"
#include "session.h"
#include "logger.h"
#include "router.h"
#include "echo_handler.h"
#include "static_handler.h"
#include "crud_api_handler.h"
#include "not_found_handler.h"
#include "sleep_handler.h"
#include "handler_registry.h"

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
      // Make sure someone actually registered this handler
      if (!HandlerRegistry::HasHandlerFor(route.handler_type)) {
        Logger::log_error("Unknown handler type: " + route.handler_type);
        return 1;
      }

      // Log that we're about to instantiate it
      Logger::log_info(
        "Instantiating handler '" + route.handler_type +
        "' for location '" + route.path + "'"
      );

      // Build a small factory that closes over the handler name
      Router::Factory factory = [name = route.handler_type](
          const std::string& loc,
          const std::unordered_map<std::string, std::string>& prms
      ) {
        return HandlerRegistry::CreateHandler(name, loc, prms);
      };

      // Register it with the router, deferring actual instantiation
      router.add_route(
        route.path,
        factory,
        route.params
      );
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

    server srv(io_service, port, router, session::MakeSession);

    std::cout << "Server running on port " << port << "\n";
    
    // Running the io_service with multiple threads
    const unsigned int num_threads = std::max(2u, std::thread::hardware_concurrency());
    Logger::log_info("Starting server with " + std::to_string(num_threads) + " threads");
    
    std::vector<std::thread> threads;
    
    // Creates worker threads (leaving one for main thread)
    for (unsigned int i = 0; i < num_threads - 1; ++i) {
        threads.emplace_back([&io_service]() {
            try {
                io_service.run();
            } catch (const std::exception& e) {
                Logger::log_error("Worker thread exception: " + std::string(e.what()));
            }
        });
    }
    
    // Running io_service in main thread as well
    try {
        io_service.run();
    } catch (const std::exception& e) {
        Logger::log_error("Main thread exception: " + std::string(e.what()));
    }
    
    // Waiting for all threads to finish
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
    Logger::log_error(std::string("Unhandled exception: ") + e.what());
  }

  return 0;
}
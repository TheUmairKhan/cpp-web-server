#ifndef LOGGER_H
#define LOGGER_H

#include <boost/log/trivial.hpp>
#include <boost/asio.hpp>
#include <string>

namespace Logger {
    //Initialize logger system with file rotation and console output
    void init_logger();

    //Get client IP address from socket
    std::string get_client_ip(const boost::asio::ip::tcp::socket& socket);

    //Logging functions at different severity levels
    void log_trace(const std::string& message);
    void log_debug(const std::string& message);
    void log_info(const std::string& message);
    void log_warning(const std::string& message);
    void log_error(const std::string& message);
    void log_fatal(const std::string& message);

    //Server-specific logging functions
    void log_server_startup(unsigned short port);
    void log_server_shutdown();
    void log_config_parsing(const std::string& filename, bool success);

    //Request-specific logging functions
    void log_request(const std::string& client_ip, const std::string& method, 
                    const std::string& uri, int status_code, const std::string& handler_type);
    void log_connection(const std::string& client_ip);

}
#endif
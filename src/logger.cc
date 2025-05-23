#include "logger.h"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/support/date_time.hpp>

#include <sstream>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace expr = boost::log::expressions;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

void Logger::init_logger() {
    logging::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

    //Console sink
    logging::add_console_log(
        std::clog,
        keywords::format = expr::stream
            << "[" << expr::attr<boost::posix_time::ptime>("TimeStamp") << "]"
            << " [T:" << expr::attr<attrs::current_thread_id::value_type>("ThreadID") << "]"
            << " [" << expr::attr<boost::log::trivial::severity_level>("Severity") << "] "
            << expr::smessage
    );

    // File sink: rotate daily at midnight or when >10MB
    logging::add_file_log(
        keywords::file_name = "logs/server_%Y-%m-%d.log",
        keywords::rotation_size = 10u * 1024u * 1024u,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0,0,0),
        keywords::auto_flush = true,
        keywords::format = expr::stream
            << "[" << expr::attr<boost::posix_time::ptime>("TimeStamp") << "]"
            << " [T:" << expr::attr<attrs::current_thread_id::value_type>("ThreadID") << "]"
            << " [" << expr::attr<boost::log::trivial::severity_level>("Severity") << "] "
            << expr::smessage
    );

    // ommon attributes: TimeStamp, ThreadID
    logging::add_common_attributes();
}

std::string Logger::get_client_ip(const boost::asio::ip::tcp::socket& socket) {
    try {
        return socket.remote_endpoint().address().to_string();
    } catch (...) {
        return "<unknown>";
    }
}

//Severity-level logging implementations using BOOST_LOG_TRIVIAL
void Logger::log_trace(const std::string& message)   { BOOST_LOG_TRIVIAL(trace)   << message; }
void Logger::log_debug(const std::string& message)   { BOOST_LOG_TRIVIAL(debug)   << message; }
void Logger::log_info(const std::string& message)    { BOOST_LOG_TRIVIAL(info)    << message; }
void Logger::log_warning(const std::string& message) { BOOST_LOG_TRIVIAL(warning) << message; }
void Logger::log_error(const std::string& message)   { BOOST_LOG_TRIVIAL(error)   << message; }
void Logger::log_fatal(const std::string& message)   { BOOST_LOG_TRIVIAL(fatal)   << message; }

//High-level server events
void Logger::log_server_startup(unsigned short port) {
    Logger::log_info("Server starting up on port " + std::to_string(port));
}
void Logger::log_server_shutdown() {
    Logger::log_info("Server shutting down");
}
void Logger::log_config_parsing(const std::string& filename, bool success) {
    Logger::log_info("Config file parsed: " + filename + (success ? " (success)" : " (failure)"));
}

//HTTP request logging
void Logger::log_connection(const std::string& client_ip) {
    Logger::log_info("New connection from " + client_ip);
}

// All logged request/response should be machine-parsable
void Logger::log_request(   
    const std::string& client_ip, 
    const std::string& method, 
    const std::string& uri, 
    int status_code, 
    const std::string& handler_type
    ) {
    std::ostringstream log_line;
    log_line <<
        "[ResponseMetrics] " <<
        "request_ip:" << client_ip << " " <<
        "request_method:" << method << " " <<
        "request_path:" << uri << " " <<
        "-> response_code:" << std::to_string(status_code) << " " <<
        "handler_type:" << handler_type;
    Logger::log_info(log_line.str());
}
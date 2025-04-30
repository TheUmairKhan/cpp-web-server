#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <boost/asio.hpp>
#include "logger.h"
#include <thread>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::init_logger();
        log_file_path = "logs/server_" + current_date() + ".log";
        // Clean up any pre-existing log file
        if (fs::exists(log_file_path)) {
            fs::remove(log_file_path);
        }
    }

    std::string log_file_path;

    std::string current_date() {
        auto t = std::time(nullptr);
        std::tm tm;
        localtime_r(&t, &tm);
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        return buffer;
    }

    std::string read_log_file() {
        std::ifstream file(log_file_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    void wait_for_log_flush() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

// Test that log file is created and contains log entries
TEST_F(LoggerTest, WritesToLogFile) {
    Logger::log_info("Test log entry");
    wait_for_log_flush();
    ASSERT_TRUE(fs::exists(log_file_path));
    std::string contents = read_log_file();
    ASSERT_NE(contents.find("Test log entry"), std::string::npos);
}

// Test get_client_ip returns string IP from mock socket
TEST_F(LoggerTest, GetClientIpReturnsValidIp) {
    boost::asio::io_service io;
    boost::asio::ip::tcp::acceptor acceptor(io, {boost::asio::ip::tcp::v4(), 0});
    boost::asio::ip::tcp::socket client(io);
    boost::asio::ip::tcp::socket server(io);

    acceptor.async_accept(server, [](const boost::system::error_code&) {});
    client.connect(acceptor.local_endpoint());
    io.run();

    std::string ip = Logger::get_client_ip(client);
    ASSERT_FALSE(ip.empty());
    ASSERT_NE(ip, "<unknown>");
}

// Test get_client_ip returns "<unknown>" when exception is thrown
TEST_F(LoggerTest, GetClientIpHandlesException) {
    boost::asio::io_service io;
    boost::asio::ip::tcp::socket dummy_socket(io);
    std::string ip = Logger::get_client_ip(dummy_socket);
    ASSERT_EQ(ip, "<unknown>");
}
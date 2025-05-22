#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <future>
#include <vector>
#include "server.h"
#include "session.h"
#include "router.h"
#include "sleep_handler.h"
#include "echo_handler.h"
#include "handler_registry.h"

using boost::asio::ip::tcp;
using namespace std::chrono;

class MultithreadingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find an available port
        tcp::acceptor temp_acceptor(io_service_, tcp::endpoint(tcp::v4(), 0));
        port_ = temp_acceptor.local_endpoint().port();
        temp_acceptor.close();
        
        // Set up router with sleep and echo handlers
        router_ = std::make_shared<Router>();
        
        // Fast sleep handler (1 second) for testing
        router_->add_route("/sleep",
                            [](const std::string& loc, const std::unordered_map<std::string, std::string>&) {
                                return HandlerRegistry::CreateHandler(SleepHandler::kName, loc, {{"sleep_duration", "1"}});
                            }, {});
        
        // Echo handler for fast responses
        router_->add_route("/echo",
                            [](const std::string& loc, const std::unordered_map<std::string, std::string>&) {
                                return HandlerRegistry::CreateHandler(EchoHandler::kName, loc, {});
                            }, {});
        
        // Start server in background thread
        server_ = std::make_unique<server>(io_service_, port_, *router_, session::MakeSession);

        // Create multiple worker threads for true concurrency testing
        const unsigned int num_threads = std::max(2u, std::thread::hardware_concurrency());
        for (unsigned int i = 0; i < num_threads; ++i) {
            server_threads_.emplace_back([this]() {
                try {
                    io_service_.run();
                } catch (...) {
                    // Ignore exceptions during shutdown
                }
            });
        }
        
        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        io_service_.stop();
        for (auto& thread : server_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    // Helper method: Make HTTP request and measure response time
    std::pair<std::string, duration<double>> make_request(const std::string& path) {
        auto start_time = high_resolution_clock::now();
        
        try {
            boost::asio::io_service client_io;
            tcp::resolver resolver(client_io);
            tcp::resolver::query query("127.0.0.1", std::to_string(port_));
            tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
            
            tcp::socket socket(client_io);
            boost::asio::connect(socket, endpoint_iterator);
            
            // Send HTTP request
            std::string request = "GET " + path + " HTTP/1.1\r\n"
                                "Host: 127.0.0.1\r\n"
                                "Connection: close\r\n\r\n";
            
            boost::asio::write(socket, boost::asio::buffer(request));
            
            // Read response
            boost::asio::streambuf response;
            boost::system::error_code error;
            boost::asio::read(socket, response, boost::asio::transfer_all(), error);
            
            auto end_time = high_resolution_clock::now();
            auto duration = duration_cast<std::chrono::duration<double>>(end_time - start_time);
            
            if (error != boost::asio::error::eof) {
                throw boost::system::system_error(error);
            }
            
            std::istream response_stream(&response);
            std::string response_str((std::istreambuf_iterator<char>(response_stream)),
                                    std::istreambuf_iterator<char>());
            
            return std::make_pair(response_str, duration);
            
        } catch (std::exception& e) {
            auto end_time = high_resolution_clock::now();
            auto duration = duration_cast<std::chrono::duration<double>>(end_time - start_time);
            return std::make_pair(std::string("ERROR: ") + e.what(), duration);
        }
    }

    // Helper method: Running multiple concurrent requests
    std::vector<std::pair<std::string, double>> run_concurrent_requests(
        const std::vector<std::string>& paths) {
        
        std::vector<std::future<std::pair<std::string, duration<double>>>> futures;
        auto start_time = high_resolution_clock::now();
        
        for (const auto& path : paths) {
            futures.push_back(std::async(std::launch::async, [this, path]() {
                return make_request(path);
            }));
        }
        
        std::vector<std::pair<std::string, double>> results;
        for (auto& future : futures) {
            auto result = future.get();
            results.emplace_back(result.first, result.second.count());
        }
        
        auto total_time = duration_cast<duration<double>>(
            high_resolution_clock::now() - start_time);
        results.emplace_back("TOTAL_TIME", total_time.count());
        
        return results;
    }

private:
    boost::asio::io_service io_service_;
    unsigned short port_;
    std::shared_ptr<Router> router_;
    std::unique_ptr<server> server_;
    std::vector<std::thread> server_threads_;
};

// Tests that multiple simultaneous requests (mix of slow and fast) are handled concurrently
// Proving the server can serve fast requests even while slow requests are running
TEST_F(MultithreadingTest, SimultaneousRequestHandling) {
    const double SLEEP_DURATION = 1.0; // In seconds
    const double TOLERANCE = 0.3; // In seconds
    
    // Launch slow request (sleep handler) in a future
    auto slow_future = std::async(std::launch::async, [this]() {
        return make_request("/sleep");
    });
    
    // Wait for slow request to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Launch fast request (echo handler) in another future  
    auto fast_future = std::async(std::launch::async, [this]() {
        return make_request("/echo");
    });
    
    // Wait for both to complete
    auto slow_result = slow_future.get();
    auto fast_result = fast_future.get();
    
    // Verify results
    EXPECT_TRUE(fast_result.first.find("200 OK") != std::string::npos) 
        << "Fast request should succeed. Got: " << fast_result.first;
    EXPECT_TRUE(slow_result.first.find("200 OK") != std::string::npos) 
        << "Slow request should succeed. Got: " << slow_result.first;
    
    // Fast request should complete much faster than slow request
    EXPECT_LT(fast_result.second.count(), SLEEP_DURATION - 0.1)
        << "Fast request took " << fast_result.second.count() 
        << " seconds, should be much less than " << SLEEP_DURATION << " seconds";
    
    EXPECT_GT(slow_result.second.count(), SLEEP_DURATION - TOLERANCE) 
        << "Slow request took " << slow_result.second.count() 
        << " seconds, should be close to " << SLEEP_DURATION << " seconds";
    
    std::cout << "Fast request duration: " << fast_result.second.count() << " seconds" << std::endl;
    std::cout << "Slow request duration: " << slow_result.second.count() << " seconds" << std::endl;
}

// Tests that multiple simultaneous requests (mix of slow and fast) are handled concurrently
// Proving the server can handle multiple blocking operations simultaneously  
TEST_F(MultithreadingTest, MultipleSimultaneousRequests) {
    const double SLEEP_DURATION = 1.0;
    
    // Launch 1 slow + 4 fast requests
    auto results = run_concurrent_requests({"/sleep", "/echo", "/echo", "/echo", "/echo"});
    
    // Extract total time (last result)
    double total_time = results.back().second;
    results.pop_back(); // Remove total time entry
    
    // Verify all requests succeeded
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_TRUE(results[i].first.find("200 OK") != std::string::npos)
            << "Request " << i << " should succeed";
    }
    
    // Total time should be close to sleep duration, not sum of all
    EXPECT_LT(total_time, SLEEP_DURATION + 1.0)
        << "Total time " << total_time 
        << " should be close to sleep duration " << SLEEP_DURATION;
    
    std::cout << "Total time for 5 simultaneous requests: " << total_time << " seconds" << std::endl;
    }

// Tests that multiple slow requests run concurrently rather than sequentially
// Verifying the server can handle multiple blocking operations within thread pool limits
TEST_F(MultithreadingTest, ConcurrentSlowRequests) {
    const double SLEEP_DURATION = 1.0;
    
    // Launch 3 slow requests
    auto results = run_concurrent_requests({"/sleep", "/sleep", "/sleep"});
    
    // Extract total time
    double total_time = results.back().second;
    results.pop_back();
    
    // Verify all requests succeeded and contain sleep message
    for (size_t i = 0; i < results.size(); ++i) {
        EXPECT_TRUE(results[i].first.find("200 OK") != std::string::npos)
            << "Slow request " << i << " should succeed";
        EXPECT_TRUE(results[i].first.find("Slept for 1 seconds") != std::string::npos)
            << "Slow request " << i << " should contain sleep message";
    }
    
    EXPECT_LT(total_time, SLEEP_DURATION * 2.0 + 0.1)
        << "Total time " << total_time << " should be less than " 
        << (SLEEP_DURATION * 2.0 + 0.1) << " seconds (2 concurrent batches of " 
        << SLEEP_DURATION << "s each with tolerance)";
    
    std::cout << "Total time for 3 concurrent slow requests: " << total_time << " seconds" << std::endl;
}

// Test that the server can handle rapid sequential requests efficiently
TEST_F(MultithreadingTest, RapidSequentialRequests) {
    const int NUM_REQUESTS = 10;
    
    auto start_time = high_resolution_clock::now();
    
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        auto result = make_request("/echo");
        EXPECT_TRUE(result.first.find("200 OK") != std::string::npos)
            << "Request " << i << " should succeed";
    }
    
    auto total_time = duration_cast<duration<double>>(
        high_resolution_clock::now() - start_time);
    
    // Should be able to handle 10 echo requests quite quickly
    EXPECT_LT(total_time.count(), 2.0)
        << "10 sequential echo requests should complete within 2 seconds, took " 
        << total_time.count() << " seconds";
    
    std::cout << "Total time for " << NUM_REQUESTS << " sequential requests: " 
                << total_time.count() << " seconds" << std::endl;
}
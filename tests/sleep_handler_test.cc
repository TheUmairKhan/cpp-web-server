#include <gtest/gtest.h>
#include <chrono>
#include "sleep_handler.h"
#include "request.h"
#include "response.h"

// -----------------------------------------------------------------------------
// SleepHandlerTest
//
// Tests SleepHandler functionality including timing, parameter handling, and proper integration with the HandlerRegistry system.
// -----------------------------------------------------------------------------
class SleepHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<SleepHandler> handler_;
  std::unique_ptr<SleepHandler> fast_handler_;

  void SetUp() override {
    // Create handler with default 5 second sleep
    RequestHandler* raw = SleepHandler::Init("/sleep", {});
    handler_.reset(static_cast<SleepHandler*>(raw));
    ASSERT_NE(handler_, nullptr) << "Init should produce a SleepHandler";
    
    // Create handler with faster 1 second sleep for quicker tests
    RequestHandler* fast_raw = SleepHandler::Init("/sleep", {{"sleep_duration", "1"}});
    fast_handler_.reset(static_cast<SleepHandler*>(fast_raw));
    ASSERT_NE(fast_handler_, nullptr) << "Init should produce a SleepHandler with custom duration";
  }
};

// -----------------------------------------------------------------------------
// Functional behavior
// -----------------------------------------------------------------------------

// Tests that a well-formed GET request sleeps for the expected duration and returns proper response
TEST_F(SleepHandlerTest, BasicSleep) {
  const std::string req = "GET /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  Response response = fast_handler_->handle_request(request);
  auto end_time = std::chrono::high_resolution_clock::now();
  
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  std::string out = response.to_string();
  EXPECT_NE(out.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(out.find("Content-Type: text/plain"), std::string::npos);
  EXPECT_EQ(response.get_status_code(), 200);
  
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_NE(out.substr(body_pos + 4).find("Slept for 1 seconds"), std::string::npos);
  
  // Should take at least 1 second (with some tolerance for system scheduling)
  EXPECT_GE(duration.count(), 900); // 0.9 seconds minimum
  EXPECT_LT(duration.count(), 2000); // but less than 2 seconds
}

// Test with custom sleep duration parameter
TEST_F(SleepHandlerTest, CustomSleepDuration) {
  // Create handler with very short sleep for testing
  RequestHandler* raw = SleepHandler::Init("/sleep", {{"sleep_duration", "0"}});
  std::unique_ptr<SleepHandler> instant_handler(static_cast<SleepHandler*>(raw));
  
  const std::string req = "GET /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  Response response = instant_handler->handle_request(request);
  auto end_time = std::chrono::high_resolution_clock::now();
  
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  std::string out = response.to_string();
  EXPECT_EQ(response.get_status_code(), 200);
  EXPECT_NE(out.substr(out.find("\r\n\r\n") + 4).find("Slept for 0 seconds"), std::string::npos);
  
  // Should complete very quickly (less than 100ms)
  EXPECT_LT(duration.count(), 100);
}

// Test with invalid sleep duration parameter (should use default)
TEST_F(SleepHandlerTest, InvalidSleepDurationUsesDefault) {
  RequestHandler* raw = SleepHandler::Init("/sleep", {{"sleep_duration", "invalid"}});
  std::unique_ptr<SleepHandler> default_handler(static_cast<SleepHandler*>(raw));
  
  const std::string req = "GET /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  
  Response response = default_handler->handle_request(request);
  std::string out = response.to_string();
  
  // Should use default 5 second sleep
  EXPECT_NE(out.substr(out.find("\r\n\r\n") + 4).find("Slept for 5 seconds"), std::string::npos);
}

// Test different HTTP methods
TEST_F(SleepHandlerTest, DifferentMethodsWork) {
  // Test with POST method
  const std::string post_req = "POST /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request post_request(post_req);
  Response post_response = fast_handler_->handle_request(post_request);
  EXPECT_EQ(post_response.get_status_code(), 200);
  
  // Test with HEAD method
  const std::string head_req = "HEAD /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request head_request(head_req);
  Response head_response = fast_handler_->handle_request(head_request);
  EXPECT_EQ(head_response.get_status_code(), 200);
}

// Test that handler works with malformed requests (should still sleep and respond)
TEST_F(SleepHandlerTest, MalformedRequestStillWorks) {
  const std::string invalid_req = "INVALID /sleep HTTP/1.1\r\nBadHeader\r\n\r\n";
  Request invalid_request(invalid_req);
  
  auto start_time = std::chrono::high_resolution_clock::now();
  Response response = fast_handler_->handle_request(invalid_request);
  auto end_time = std::chrono::high_resolution_clock::now();
  
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Should still return 200 and sleep (SleepHandler is not picky about request validity)
  EXPECT_EQ(response.get_status_code(), 200);
  EXPECT_GE(duration.count(), 900); // Should still sleep
}

// Tests zero sleep duration parameter
TEST_F(SleepHandlerTest, ZeroSleepDuration) {
   RequestHandler* raw = SleepHandler::Init("/sleep", {{"sleep_duration", "0"}});
   std::unique_ptr<SleepHandler> zero_handler(static_cast<SleepHandler*>(raw));
   
   const std::string req = "GET /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
   Request request(req);
   
   auto start_time = std::chrono::high_resolution_clock::now();
   Response response = zero_handler->handle_request(request);
   auto end_time = std::chrono::high_resolution_clock::now();
   
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
   
   EXPECT_EQ(response.get_status_code(), 200);
   EXPECT_NE(response.to_string().find("Slept for 0 seconds"), std::string::npos);
   EXPECT_LT(duration.count(), 50);
}

// Tests negative sleep duration parameter (should be treated as zero)
TEST_F(SleepHandlerTest, NegativeSleepDuration) {
   RequestHandler* raw = SleepHandler::Init("/sleep", {{"sleep_duration", "-5"}});
   std::unique_ptr<SleepHandler> negative_handler(static_cast<SleepHandler*>(raw));
   
   const std::string req = "GET /sleep HTTP/1.1\r\nHost: localhost\r\n\r\n";
   Request request(req);
   
   auto start_time = std::chrono::high_resolution_clock::now();
   Response response = negative_handler->handle_request(request);
   auto end_time = std::chrono::high_resolution_clock::now();
   
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
   
   EXPECT_EQ(response.get_status_code(), 200);
   EXPECT_LT(duration.count(), 50);
}
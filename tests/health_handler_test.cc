#include <gtest/gtest.h>
#include "health_handler.h"
#include "request.h"
#include "response.h"

// -----------------------------------------------------------------------------
// HealthHandlerTest
//
// Tests the HealthHandler which should always return a 200 response.
// -----------------------------------------------------------------------------
class HealthHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<HealthHandler> handler_;

  void SetUp() override {
    RequestHandler* raw = HealthHandler::Init("/health", {});
    handler_.reset(static_cast<HealthHandler*>(raw));
    ASSERT_NE(handler_, nullptr) << "Init should produce a HealthHandler";
  }
};

// -----------------------------------------------------------------------------
// Basic Response Test
// -----------------------------------------------------------------------------

// All requests should receive a 200 response
TEST_F(HealthHandlerTest, BasicResponse) {
  const std::string req = "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(out.find("Content-Type: text/plain"), std::string::npos);
  EXPECT_EQ(response.get_status_code(), 200);
  
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_NE(out.substr(body_pos + 4).find("OK"), std::string::npos);
}

// Different request methods should all receive 200
TEST_F(HealthHandlerTest, DifferentMethodsReturn200) {
  // Test with POST method
  const std::string post_req = "POST /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request post_request(post_req);
  Response post_response = handler_->handle_request(post_request);
  EXPECT_EQ(post_response.get_status_code(), 200);
  
  // Test with HEAD method
  const std::string head_req = "HEAD /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request head_request(head_req);
  Response head_response = handler_->handle_request(head_request);
  EXPECT_EQ(head_response.get_status_code(), 200);
}

// Requests with extra path segments should return 200
TEST_F(HealthHandlerTest, RequestWithLongPathStillReturns200) {
  // Request with invalid format
  const std::string long_path_req = "GET /health/extra/path/stuff HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(long_path_req);
  Response response = handler_->handle_request(request);

  EXPECT_EQ(response.get_status_code(), 200);
}
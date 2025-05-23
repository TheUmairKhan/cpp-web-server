#include <gtest/gtest.h>
#include "not_found_handler.h"
#include "request.h"
#include "response.h"

// -----------------------------------------------------------------------------
// NotFoundHandlerTest
//
// Tests the NotFoundHandler which should always return a 404 response.
// -----------------------------------------------------------------------------
class NotFoundHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<NotFoundHandler> handler_;

  void SetUp() override {
    RequestHandler* raw = NotFoundHandler::Init("/", {});
    handler_.reset(static_cast<NotFoundHandler*>(raw));
    ASSERT_NE(handler_, nullptr) << "Init should produce a NotFoundHandler";
  }
};

// -----------------------------------------------------------------------------
// Basic 404 Response Test
// -----------------------------------------------------------------------------

// All requests should receive a 404 response
TEST_F(NotFoundHandlerTest, Basic404Response) {
  const std::string req = "GET /path/not/found HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 404 Not Found"), std::string::npos);
  EXPECT_NE(out.find("Content-Type: text/plain"), std::string::npos);
  EXPECT_EQ(response.get_status_code(), 404);
  
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_NE(out.substr(body_pos + 4).find("404 Not Found"), std::string::npos);
}

// Different request methods should all receive 404
TEST_F(NotFoundHandlerTest, DifferentMethodsReturn404) {
  // Test with POST method
  const std::string post_req = "POST /some/path HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request post_request(post_req);
  Response post_response = handler_->handle_request(post_request);
  EXPECT_EQ(post_response.get_status_code(), 404);
  
  // Test with HEAD method
  const std::string head_req = "HEAD /some/path HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request head_request(head_req);
  Response head_response = handler_->handle_request(head_request);
  EXPECT_EQ(head_response.get_status_code(), 404);
}

// Even invalid requests should receive 404 (not 400)
TEST_F(NotFoundHandlerTest, InvalidRequestStillReturns404) {
  // Request with invalid format
  const std::string invalid_req = "INVALID / HTTP/1.1\r\nBadHeader\r\n\r\n";
  Request invalid_request(invalid_req);
  Response response = handler_->handle_request(invalid_request);
  
  // Should still be 404, not 400 Bad Request
  EXPECT_EQ(response.get_status_code(), 404);
}
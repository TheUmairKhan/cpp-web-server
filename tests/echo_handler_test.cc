#include <gtest/gtest.h>
#include "echo_handler.h"
#include "request.h"
#include "response.h"

// -----------------------------------------------------------------------------
// EchoHandlerTest
//
// Instantiates EchoHandler directly via its Init factory and verifies behavior.
// -----------------------------------------------------------------------------
class EchoHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<EchoHandler> handler_;

  void SetUp() override {
    RequestHandler* raw = EchoHandler::Init("/echo", {});
    handler_.reset(static_cast<EchoHandler*>(raw));
    ASSERT_NE(handler_, nullptr) << "Init should produce an EchoHandler";
  }
};

// -----------------------------------------------------------------------------
// Functional behavior
// -----------------------------------------------------------------------------

// A well-formed GET should echo the entire request back as the body.
TEST_F(EchoHandlerTest, BasicEcho) {
  const std::string req = "GET /foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(out.find("Content-Type: text/plain"), std::string::npos);
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(out.substr(body_pos + 4), req);
}

// LF-only line breaks should still be accepted and echoed.
TEST_F(EchoHandlerTest, LFTest) {
  const std::string req = "GET /foo HTTP/1.1\nHost: localhost\n\n";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 200 OK"), std::string::npos);
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(out.substr(body_pos + 4), req);
}

// POST requests are not supported and must return 400.
TEST_F(EchoHandlerTest, PostRequestReturns400) {
  const std::string req =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n"
      "Body data that should be ignored";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 400 Bad Request"), std::string::npos);
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(out.substr(body_pos + 4), "Bad Request");
}

// HEAD requests should behave like GET and echo back the request.
TEST_F(EchoHandlerTest, HeadRequestReturns200) {
  const std::string req =
      "HEAD / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";
  Request request(req);
  Response response = handler_->handle_request(request);
  std::string out = response.to_string();

  EXPECT_NE(out.find("HTTP/1.1 200 OK"), std::string::npos);
  auto body_pos = out.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  EXPECT_EQ(out.substr(body_pos + 4), req);
}

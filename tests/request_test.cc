#include <gtest/gtest.h>
#include "request.h"

// ----------  RequestTest Fixture  ---------------
class RequestTest : public ::testing::Test {
  protected:
    std::string req;
    std::unique_ptr<Request> request;
};

// ----------------- Request unit tests -----------------
TEST_F(RequestTest, ValidRequest) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_TRUE(request->is_valid());
  EXPECT_EQ(request->get_method(), "GET");
  EXPECT_EQ(request->get_url(), "/foo");
  EXPECT_EQ(request->get_version(), "HTTP/1.1");
  EXPECT_EQ(request->get_header("Host"), "localhost:8080");
  EXPECT_EQ(request->get_header("Accept"), "*/*");
}

TEST_F(RequestTest, BadRequestExtraSpaces) {
  req = "GET    /foo   HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_FALSE(request->is_valid());
}

TEST_F(RequestTest, ValidRequestNewline) {
  req = "GET /foo HTTP/1.1\nHost: localhost:8080\nAccept: */*\n\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_TRUE(request->is_valid());
  EXPECT_EQ(request->get_method(), "GET");
  EXPECT_EQ(request->get_url(), "/foo");
  EXPECT_EQ(request->get_version(), "HTTP/1.1");
  EXPECT_EQ(request->get_header("Host"), "localhost:8080");
  EXPECT_EQ(request->get_header("Accept"), "*/*");
}

TEST_F(RequestTest, RequestNoEnd) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  // Although the request does not terminate properly, it is still parsed
  // requests with no end will be caught in session
  ASSERT_TRUE(request->is_valid());
  EXPECT_EQ(request->get_method(), "GET");
  EXPECT_EQ(request->get_url(), "/foo");
  EXPECT_EQ(request->get_version(), "HTTP/1.1");
  EXPECT_EQ(request->get_header("Host"), "localhost:8080");
  EXPECT_EQ(request->get_header("Accept"), "*/*");
}

TEST_F(RequestTest, BadRequestNoMethod) {
  req = "/foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestBadMethod) {
  req = "HELLO /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestNoURL) {
  req = "GET HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestNoVersion) {
  req = "GET /foo\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestBadVersion) {
  req = "GET /foo http/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestBadHeader) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  EXPECT_FALSE(request->is_valid());
}

TEST_F(RequestTest, GoodRequestWrongHeader) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: */*\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Try get_header with bad header
  // Should return empty string
  EXPECT_EQ(request->get_header("Bad"), "");
}

TEST_F(RequestTest, GoodRequestEmptyHeader) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: \r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_TRUE(request->is_valid());
  EXPECT_EQ(request->get_method(), "GET");
  EXPECT_EQ(request->get_url(), "/foo");
  EXPECT_EQ(request->get_version(), "HTTP/1.1");
  EXPECT_EQ(request->get_header("Host"), "localhost:8080");
  EXPECT_EQ(request->get_header("Accept"), "");
}

TEST_F(RequestTest, GoodRequestHeaderHasSpace) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\nAccept: text/html, application/xhtml+xml\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_TRUE(request->is_valid());
  EXPECT_EQ(request->get_method(), "GET");
  EXPECT_EQ(request->get_url(), "/foo");
  EXPECT_EQ(request->get_version(), "HTTP/1.1");
  EXPECT_EQ(request->get_header("Host"), "localhost:8080");
  EXPECT_EQ(request->get_header("Accept"), "text/html, application/xhtml+xml");
}

TEST_F(RequestTest, BadRequestHeaderNameHasSpace) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost:8080\r\n Bad: test\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_FALSE(request->is_valid());
}

TEST_F(RequestTest, BadRequestDuplicateHeader) {
  req = "GET /foo HTTP/1.1\r\nBad: localhost:8080\r\nBad: test\r\n\r\n";
  request = std::make_unique<Request>(req);

  // Check fields
  ASSERT_FALSE(request->is_valid());
}

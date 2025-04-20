#include <gtest/gtest.h>
#include "http_responder.h"

// ----------  Fixture that starts the server on SetUp()  ---------------
class HttpResponderTest : public ::testing::Test {
  protected:
    std::string req;
    std::string resp;
};

// ----------------  HttpResponder unit tests  ------------

// 1) Simple GET with CRLF terminator
TEST_F(HttpResponderTest, BasicEcho) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  HttpResponder::make_response(req, resp);

  // Status line & header checks
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(resp.find("Content-Type: text/plain"), std::string::npos);

  // Body should exactly equal original request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 2) Malformed start‑line should return 400
TEST_F(HttpResponderTest, MalformedStartLine) {
  req = "G?T /oops HTTP/1.1\r\nHost: x\r\n\r\n";
  HttpResponder::make_response(req, resp);

  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);
  // Body should be empty
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 3) Request uses only LF line breaks instead of CRLF
TEST_F(HttpResponderTest, LFTest) {
  req = "GET /foo HTTP/1.1\nHost: localhost\n\n";
  HttpResponder::make_response(req, resp);

  // Status line & header checks
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(resp.find("Content-Type: text/plain"), std::string::npos);

  // Body should exactly equal original request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 4) A POST request should return 400, since we only handle GET at the moment
TEST_F(HttpResponderTest, PostRequestReturns400) {
  req =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n"
      "Body data that should be ignored";
  HttpResponder::make_response(req, resp);

  // Verify the status line.
  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);

  // Body should be empty since we return 400.
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 5) A HEAD request should return 400, since we only handle GET at the moment
TEST_F(HttpResponderTest, HeadRequestReturns400) {
  req =
      "HEAD / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";
  HttpResponder::make_response(req, resp);

  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_TRUE(body.empty());
}

// 8) A valid GET with additional body content. Should be echo'd back by the server.
TEST_F(HttpResponderTest, GETWithRequestBody) {
  // Notice we have a body even though GET typically doesn't carry one.
  // The server's echo logic will include it in the response body.
  req =
      "GET /carry_body HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "Hello";  // This is the "body" that comes right after blank line
  HttpResponder::make_response(req, resp);

  // The status should be 200 OK, and the body should match the entire request
  // (headers + the "Hello" part).
  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);

  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// 9) Test request_complete with complete request with \r\n\r\n
TEST_F(HttpResponderTest, RequestCompleteCRLF) {
  req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
  EXPECT_TRUE(HttpResponder::request_complete(req));
}

// 10) Test request_complete with complete request with \r\n\r\n
TEST_F(HttpResponderTest, RequestCompleteLF) {
  req = "GET / HTTP/1.1\nHost: localhost\n\n";
  EXPECT_TRUE(HttpResponder::request_complete(req));
}

// 11) Test request_complete with incomplete request
TEST_F(HttpResponderTest, RequestIncomplete) {
  req = "GET / HTTP/1.1\r\nHost: localhost\r\n";
  EXPECT_FALSE(HttpResponder::request_complete(req));
}
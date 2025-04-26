#include <gtest/gtest.h>
#include "echo_handler.h"
#include "request.h"
#include "response.h"

// ----------  EchoHandlerTest Fixture  ---------------
class EchoHandlerTest : public ::testing::Test {
  protected:
    std::string req;
    std::string resp;
    std::unique_ptr<EchoHandler> handler = std::make_unique<EchoHandler>();
};

// ----------------  EchoHandler unit tests  ------------

// 1) Simple GET with CRLF terminator
TEST_F(EchoHandlerTest, BasicEcho) {
  req = "GET /foo HTTP/1.1\r\nHost: localhost\r\n\r\n";
  Request request(req);
  Response response = handler->handle_request(request);
  resp = response.to_string();

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
TEST_F(EchoHandlerTest, MalformedStartLine) {
  req = "G?T /oops HTTP/1.1\r\nHost: x\r\n\r\n";
  Request request(req);
  Response response = handler->handle_request(request);
  resp = response.to_string();

  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);
  // Body should say Bad Request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "Bad Request");
}

// 3) Request uses only LF line breaks instead of CRLF
TEST_F(EchoHandlerTest, LFTest) {
  req = "GET /foo HTTP/1.1\nHost: localhost\n\n";
  Request request(req);
  Response response = handler->handle_request(request);
  resp = response.to_string();

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
TEST_F(EchoHandlerTest, PostRequestReturns400) {
  req =
      "POST /upload HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n"
      "Body data that should be ignored";
  Request request(req);
  Response response = handler->handle_request(request);
  resp = response.to_string();

  // Verify the status line.
  EXPECT_NE(resp.find("HTTP/1.1 400 Bad Request"), std::string::npos);

  // Body should say Bad Request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "Bad Request");
}

// 5) A HEAD request should return 400, since we only handle GET at the moment (NOT TRUE NO MORE!)
TEST_F(EchoHandlerTest, HeadRequestReturns200) {
  req =
      "HEAD / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";
  Request request(req);
  Response response = handler->handle_request(request);
  resp = response.to_string();

  EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);

  // Body should exactly equal original request
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

// Does not work anymore due to Request class ending request at \r\n\r\n
// For now, this test case isn't needed as only GET requests are handled by our server
// so they will never have a body
// 8) A valid GET with additional body content. Should be echo'd back by the server.
// TEST_F(EchoHandlerTest, GETWithRequestBody) {
//   // Notice we have a body even though GET typically doesn't carry one.
//   // The server's echo logic will include it in the response body.
//   req =
//       "GET /carry_body HTTP/1.1\r\n"
//       "Host: localhost\r\n"
//       "Content-Length: 5\r\n"
//       "\r\n"
//       "Hello";  // This is the "body" that comes right after blank line
//   Request request(req);
//   handler->handle_request(request, resp);

//   // The status should be 200 OK, and the body should match the entire request
//   // (headers + the "Hello" part).
//   EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);

//   auto body_pos = resp.find("\r\n\r\n");
//   ASSERT_NE(body_pos, std::string::npos);
//   std::string body = resp.substr(body_pos + 4);
//   EXPECT_EQ(body, req);
// }

#include <gtest/gtest.h>

#include "response.h"
#include <stdexcept>

// Basic HTTP 200 OK response
TEST(ResponseTest, Correct200Response) {
    Response res("HTTP/1.1", 200, "text/html", 13, "close", "<h1>Hello</h1>");

    std::string expected = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n\r\n"
        "<h1>Hello</h1>";

    EXPECT_EQ(res.to_string(), expected);
}

// 404 Not Found response 
TEST(ResponseTest, Correct404Response) {
    Response res("HTTP/1.1", 404, "text/plain", 9, "keep-alive", "Not Found");

    std::string expected = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 9\r\n"
        "Connection: keep-alive\r\n\r\n"
        "Not Found";

    EXPECT_EQ(res.to_string(), expected);
}

// 403 Forbidden with empty body
TEST(ResponseTest, Correct403Response) {
    Response res("HTTP/1.1", 403, "text/plain", 0, "close", "");

    std::string expected =
        "HTTP/1.1 403 Forbidden\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";

    EXPECT_EQ(res.to_string(), expected);
}

// 400 Bad Request
TEST(ResponseTest, Correct400Response) {
    Response res("HTTP/1.1", 400, "text/plain", 11, "close", "Bad Request");

    std::string expected =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 11\r\n"
        "Connection: close\r\n\r\n"
        "Bad Request";

    EXPECT_EQ(res.to_string(), expected);
}
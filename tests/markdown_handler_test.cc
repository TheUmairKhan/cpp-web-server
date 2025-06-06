// markdown_handler_test.cc

#include <gtest/gtest.h>
#include "markdown_handler.h"
#include "request.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class MarkdownHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        temp_dir_ = fs::temp_directory_path() / "markdown_test";
        fs::create_directories(temp_dir_);

        // Instantiate the handler via the factory, with prefix "/markdown" and root = temp_dir_
        RequestHandler* raw = MarkdownHandler::Init(
            "/markdown",
            {{ "root", temp_dir_.string() }}
        );
        handler_.reset(static_cast<MarkdownHandler*>(raw));

        // Create a valid Markdown file: test.md
        std::string md_body =
            "# Integration Test\n"
            "This is a test markdown file.\n"
            "- 1\n"
            "- 2\n";
        create_test_file("test.md", md_body);

        // Create a non‐Markdown file: test.txt
        std::string txt_body = "This is a plain text file.";
        create_test_file("test.txt", txt_body);

        // Create a file outside the root, to test traversal:
        // i.e. ../secret.md
        fs::path parent = temp_dir_.parent_path();
        fs::path secret_file = parent / "secret.md";
        std::ofstream(secret_file.c_str()) << "# Secret\nYou shouldn't see this.";
        secret_path_ = secret_file.string();
    }

    void TearDown() override {
        // Clean up: remove both the temporary directory and the "secret.md"
        fs::remove_all(temp_dir_);
        fs::path parent = temp_dir_.parent_path();
        fs::remove(parent / "secret.md");
    }

    void create_test_file(const std::string& name,
                          const std::string& content) {
        std::ofstream((temp_dir_ / name).c_str()) << content;
    }

    fs::path temp_dir_;
    std::string secret_path_;
    std::unique_ptr<MarkdownHandler> handler_;
};

//
// 1) Existing test: valid Markdown file → 200 OK + converted HTML
//
TEST_F(MarkdownHandlerTest, ServeMarkdownFile) {
    // Arrange: Request the existing test.md under /markdown
    Request request("GET /markdown/test.md HTTP/1.1\r\nHost: localhost\r\n\r\n");

    // Act
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    // Extract the body (after CRLF CRLF)
    auto body_pos = resp_str.find("\r\n\r\n");
    ASSERT_NE(body_pos, std::string::npos) << "No header/body separation found";
    std::string body = resp_str.substr(body_pos + 4);

    // Expected HTML output via the same markdown converter
    std::string expected_body =
        markdown::WrapInHtmlTemplate(
            markdown::ConvertToHtml(
                "# Integration Test\n"
                "This is a test markdown file.\n"
                "- 1\n"
                "- 2\n"
            )
        );

    // Assert
    EXPECT_NE(resp_str.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp_str.find("Content-Type: text/html"), std::string::npos);
    EXPECT_EQ(body, expected_body);
}

//
// 2) Existing test: non‐existent Markdown → 404 Not Found
//
TEST_F(MarkdownHandlerTest, MissingMarkdownFileReturns404) {
    // Arrange: Request a missing .md file under /markdown
    Request request("GET /markdown/missing.md HTTP/1.1\r\nHost: localhost\r\n\r\n");

    // Act
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    // Assert status 404
    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

//
// 3) New: requesting a non‐.md file under the valid prefix → 400 Bad Request
//
TEST_F(MarkdownHandlerTest, NonMarkdownExtensionReturns400) {
    // Arrange: test.txt exists under temp_dir_, but extension is .txt
    Request request("GET /markdown/test.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");

    // Act
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    // Assert that it returns "400 Bad Request" and not "200" or "404"
    EXPECT_NE(resp_str.find("HTTP/1.1 400 Bad Request"), std::string::npos);
    EXPECT_EQ(resp_str.find("Content-Type: text/html"), std::string::npos)
        << "Should not serve non‐MD as HTML";
}

//
// 4) New: path traversal attempt (e.g. "/markdown/../secret.md") → 404 Not Found
//
TEST_F(MarkdownHandlerTest, PathTraversalAttemptReturns404) {
    // Arrange: Construct a URL that tries to go one level up: "/markdown/../secret.md"
    Request request("GET /markdown/../secret.md HTTP/1.1\r\nHost: localhost\r\n\r\n");

    // Act
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    // Assert: Should be treated as 404 Not Found (no exposure of secret.md)
    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

//
// 5) New: wrong URL prefix (e.g. "/other/test.md") → 404 Not Found
//
TEST_F(MarkdownHandlerTest, WrongPrefixReturns404) {
    // Arrange: Create a Markdown file under the real filesystem, but request under "/other"
    create_test_file("other.md", "# Other\nContent");
    Request request("GET /other/other.md HTTP/1.1\r\nHost: localhost\r\n\r\n");

    // Act
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    // Assert: since the handler only watches "/markdown", this should be 404
    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

//
// 6) New: malformed URL (no leading slash, or entirely wrong) → 404
//
TEST_F(MarkdownHandlerTest, MalformedUrlReturns404) {
    // Example 1: missing leading slash
    Request request1("GET markdown/test.md HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response1 = handler_->handle_request(request1);
    std::string resp1 = response1.to_string();
    EXPECT_NE(resp1.find("HTTP/1.1 404 Not Found"), std::string::npos);

    // Example 2: missing prefix entirely, but valid-looking path
    Request request2("GET /random/test.md HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response2 = handler_->handle_request(request2);
    std::string resp2 = response2.to_string();
    EXPECT_NE(resp2.find("HTTP/1.1 404 Not Found"), std::string::npos);
}


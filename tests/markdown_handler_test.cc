#include <gtest/gtest.h>
#include "markdown_handler.h"
#include "request.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class MarkdownHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "markdown_test";
        fs::create_directories(temp_dir_);

        // call the factory, then cast its RequestHandler* to StaticHandler*
        RequestHandler* raw = MarkdownHandler::Init(
            "/markdown",
            {{ "root", temp_dir_.string() }}
        );

        handler_.reset(static_cast<MarkdownHandler*>(raw));

        std::string md_body =   "# Integration Test\n"
                                "This is a test markdown file.\n"
                                "- 1\n"
                                "- 2";

        create_test_file("test.md",  md_body);
    }

    void TearDown() override {
        fs::remove_all(temp_dir_);
    }

    void create_test_file(const std::string& name,
                          const std::string& content) {
        std::ofstream((temp_dir_ / name).c_str()) << content;
    }

    fs::path temp_dir_;
    std::unique_ptr<MarkdownHandler> handler_;
};

TEST_F(MarkdownHandlerTest, ServeFile) {
    Request request("GET /markdown/test.md HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();
    auto body_pos = resp_str.find("\r\n\r\n");
    auto body = resp_str.substr(body_pos + 4);
    auto expected_body =    markdown::WrapInHtmlTemplate(
                                    markdown::ConvertToHtml(
                                        "# Integration Test\n"
                                        "This is a test markdown file.\n"
                                        "- 1\n"
                                        "- 2"));

    EXPECT_NE(resp_str.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp_str.find("Content-Type: text/html"), std::string::npos);
    EXPECT_EQ(body, expected_body);
}

TEST_F(MarkdownHandlerTest, FileNotFound) {
    Request request("GET /markdown/missing.md HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}
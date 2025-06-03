#include <gtest/gtest.h>
#include "static_handler.h"
#include "request.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class StaticHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "static_test";
        fs::create_directories(temp_dir_);

        // call the factory, then cast its RequestHandler* to StaticHandler*
        RequestHandler* raw = StaticHandler::Init(
            "/static",
            {{ "root", temp_dir_.string() }}
        );

        handler_.reset(static_cast<StaticHandler*>(raw));

        create_test_file("test.txt",  "Sample text");
        create_test_file("image.jpg", "Fake JPEG\xFF\xD8\xFF");
    }

    void TearDown() override {
        fs::remove_all(temp_dir_);
    }

    void create_test_file(const std::string& name,
                          const std::string& content) {
        std::ofstream((temp_dir_ / name).c_str()) << content;
    }

    fs::path temp_dir_;
    std::unique_ptr<StaticHandler> handler_;
};

TEST_F(StaticHandlerTest, ServeTextFile) {
    Request request("GET /static/test.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    EXPECT_NE(resp_str.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp_str.find("Content-Type: text/plain"), std::string::npos);
    EXPECT_NE(resp_str.find("Sample text"), std::string::npos);
}

TEST_F(StaticHandlerTest, ServeImageFile) {
    Request request("GET /static/image.jpg HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    EXPECT_NE(resp_str.find("Content-Type: image/jpeg"), std::string::npos);
    EXPECT_NE(resp_str.find("Fake JPEG"), std::string::npos);
}

TEST_F(StaticHandlerTest, FileNotFound) {
    Request request("GET /static/missing.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}

TEST_F(StaticHandlerTest, PathTraversalBlocked) {
    Request request("GET /static/../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
    Response response = handler_->handle_request(request);
    std::string resp_str = response.to_string();

    EXPECT_NE(resp_str.find("HTTP/1.1 404 Not Found"), std::string::npos);
}
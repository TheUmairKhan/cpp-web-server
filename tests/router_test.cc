#include <gtest/gtest.h>
#include <fstream>

#include "router.h"
#include "echo_handler.h"
#include "static_handler.h"
#include "request.h"
#include "response.h"

namespace fs = std::filesystem;

class RouterTest : public ::testing::Test {
protected:
  void SetUp() override {
    router_ = std::make_shared<Router>();
    temp_dir_ = fs::temp_directory_path() / "static_test";
    fs::create_directories(temp_dir_);
    StaticHandler::configure("/static_test", temp_dir_.string());
    create_test_file("test.txt", "this is a test");
  }

  void create_test_file(const std::string& name, const std::string& content) {
    std::ofstream((temp_dir_ / name).c_str()) << content;
  }

  std::shared_ptr<Router> router_;
  fs::path temp_dir_;
};

// Add multiple routes and test that they are all sanitized
TEST_F(RouterTest, AddRoutes) {
  router_->add_route("", std::make_unique<EchoHandler>());
  router_->add_route("/foo", std::make_unique<EchoHandler>());
  router_->add_route("goo", std::make_unique<EchoHandler>());
  router_->add_route("/public/", std::make_unique<StaticHandler>());
  std::vector<std::string> routes = router_->get_routes();
  ASSERT_EQ(routes.size(), 4);
  EXPECT_EQ(routes[0], "/");
  EXPECT_EQ(routes[1], "/foo");
  EXPECT_EQ(routes[2], "/goo");
  EXPECT_EQ(routes[3], "/public");
}

TEST_F(RouterTest, HandleStaticRequest) {
  router_->add_route("/static_test", std::make_unique<StaticHandler>());
  std::string req = "GET /static_test/test.txt HTTP/1.1\r\n\r\n";
  std::unique_ptr<Request> request = std::make_unique<Request>(req);
  Response response = router_->handle_request(*request);
  std::string resp = response.to_string();
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "this is a test");
}

TEST_F(RouterTest, HandleEchoRequest) {
  router_->add_route("/echo", std::make_unique<EchoHandler>());
  std::string req = "GET /echo/hello HTTP/1.1\r\n\r\n";
  std::unique_ptr<Request> request = std::make_unique<Request>(req);
  Response response = router_->handle_request(*request);
  std::string resp = response.to_string();
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, req);
}

TEST_F(RouterTest, HandleBadRequest) {
  std::string req = "GET /unknown/bad HTTP/1.1\r\n\r\n";
  std::unique_ptr<Request> request = std::make_unique<Request>(req);
  Response response = router_->handle_request(*request);
  std::string resp = response.to_string();
  auto body_pos = resp.find("\r\n\r\n");
  ASSERT_NE(body_pos, std::string::npos);
  std::string body = resp.substr(body_pos + 4);
  EXPECT_EQ(body, "Not Found");
}
  
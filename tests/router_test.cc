#include <gtest/gtest.h>
#include <fstream>

#include "router.h"
#include "handler_registry.h"
#include "echo_handler.h"
#include "static_handler.h"
#include "request.h"
#include "response.h"

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// RouterTest fixture
//
// Prepares a Router instance and temporary filesystem for static file tests.
// -----------------------------------------------------------------------------
class RouterTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create a fresh Router and temp directory for each test
    router_   = std::make_shared<Router>();
    temp_dir_ = fs::temp_directory_path() / "static_test";
    fs::create_directories(temp_dir_);
    create_test_file("test.txt", "this is a test");
  }

  // Helper to write a test file into our temp directory
  void create_test_file(const std::string& name,
                        const std::string& content) {
    std::ofstream((temp_dir_ / name).c_str()) << content;
  }

  std::shared_ptr<Router> router_;
  fs::path              temp_dir_;
};

// -----------------------------------------------------------------------------
// Helper: make_factory
//
// Returns a factory that wraps HandlerRegistry::CreateHandler for a given name.
// -----------------------------------------------------------------------------
static HandlerRegistry::RequestHandlerFactory
make_factory(const std::string& handler_name) {
  return [handler_name](const std::string& loc,
                        const std::unordered_map<std::string,std::string>& params) {
    return HandlerRegistry::CreateHandler(handler_name, loc, params);
  };
}

// -----------------------------------------------------------------------------
// Test: AddRoutes
//
// Verify that sanitize_path is applied, duplicates are allowed, and ordering.
// -----------------------------------------------------------------------------
TEST_F(RouterTest, AddRoutes) {
  // Register EchoHandler at various prefixes
  router_->add_route("",    make_factory(EchoHandler::kName), {});
  router_->add_route("/foo", make_factory(EchoHandler::kName), {});
  router_->add_route("goo",  make_factory(EchoHandler::kName), {});

  // Register StaticHandler at "/public"
  router_->add_route("/public/",
                     make_factory(StaticHandler::kName),
                     {{"root", temp_dir_.string()}});

  auto routes = router_->get_routes();
  ASSERT_EQ(routes.size(), 4u);
  EXPECT_EQ(routes[0], "/");
  EXPECT_EQ(routes[1], "/foo");
  EXPECT_EQ(routes[2], "/goo");
  EXPECT_EQ(routes[3], "/public");
}

// -----------------------------------------------------------------------------
// Test: HandleStaticRequest
//
// Ensure StaticHandler serves files correctly via the registry factory.
// -----------------------------------------------------------------------------
TEST_F(RouterTest, HandleStaticRequest) {
  router_->add_route("/static_test",
                     make_factory(StaticHandler::kName),
                     {{"root", temp_dir_.string()}});
  std::string req = "GET /static_test/test.txt HTTP/1.1\r\n\r\n";
  Response resp = router_->handle_request(Request(req));
  auto s = resp.to_string();
  auto p = s.find("\r\n\r\n");
  ASSERT_NE(p, std::string::npos);
  EXPECT_EQ(s.substr(p+4), "this is a test");
}

// -----------------------------------------------------------------------------
// Test: HandleEchoRequest
//
// Verify that EchoHandler echoes the request body as expected.
// -----------------------------------------------------------------------------
TEST_F(RouterTest, HandleEchoRequest) {
  router_->add_route("/echo",
                     make_factory(EchoHandler::kName),
                     {});
  std::string req = "GET /echo/hello HTTP/1.1\r\n\r\n";
  Response resp = router_->handle_request(Request(req));
  auto s = resp.to_string();
  auto p = s.find("\r\n\r\n");
  ASSERT_NE(p, std::string::npos);
  EXPECT_EQ(s.substr(p+4), req);
}

// -----------------------------------------------------------------------------
// Test: HandleBadRequest
//
// A request for an unregistered prefix should return 404 Not Found.
// -----------------------------------------------------------------------------
TEST_F(RouterTest, HandleBadRequest) {
  std::string req = "GET /unknown/bad HTTP/1.1\r\n\r\n";
  Response resp = router_->handle_request(Request(req));
  auto s = resp.to_string();
  auto p = s.find("\r\n\r\n");
  ASSERT_NE(p, std::string::npos);
  EXPECT_EQ(s.substr(p+4), "Not Found");
}

// -----------------------------------------------------------------------------
// Lifetime test: HandlersArePerRequest
//
// Ensure each handle_request call creates and destroys its own handler.
// This avoids shared-state/thread-safety issues.
// -----------------------------------------------------------------------------
static int g_live_count = 0;
struct TrackingHandler : RequestHandler {
  TrackingHandler()            { ++g_live_count; }
  ~TrackingHandler() override  { --g_live_count; }
  Response handle_request(const Request& req) override {
    return Response(req.get_version(), 200, "text/plain", 0, "close", "");
  }
};

TEST(RouterLifetimeTest, HandlersArePerRequest) {
  Router r;
  // Register a route whose factory yields a new TrackingHandler each time
  r.add_route("/",
              [](const std::string& /*loc*/,
                 const std::unordered_map<std::string,std::string>& /*params*/) {
                return new TrackingHandler();
              },
              {});
  Request req("GET / HTTP/1.1\r\nHost: x\r\n\r\n");

  // No live handlers before any request
  EXPECT_EQ(g_live_count, 0);

  // After each request, handler count must return to zero
  r.handle_request(req);
  EXPECT_EQ(g_live_count, 0);

  r.handle_request(req);
  EXPECT_EQ(g_live_count, 0);
}

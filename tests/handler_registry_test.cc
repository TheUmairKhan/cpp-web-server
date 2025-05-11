#include "handler_registry.h"

#include "request_handler.h"
#include "echo_handler.h"     
#include "static_handler.h"   
#include <gtest/gtest.h>
#include <stdexcept>

// -----------------------------------------------------------------------------
// DummyHandler
//
// A trivial RequestHandler for testing registry behavior. We never invoke
// handle_request, so throwing is acceptable.
// -----------------------------------------------------------------------------
class DummyHandler : public RequestHandler {
 public:
  Response handle_request(const Request& /*req*/) override {
    throw std::logic_error("DummyHandler should not be used here");
  }
};

// -----------------------------------------------------------------------------
// RegisterHandler tests
// -----------------------------------------------------------------------------

// First registration under a given name should succeed; duplicates should fail.
TEST(HandlerRegistryTest, RegisterReturnsTrueThenFalseOnDuplicate) {
  bool first = HandlerRegistry::RegisterHandler(
      "Foo",
      [](const std::string& /*loc*/,
         const std::unordered_map<std::string, std::string>& /*params*/) {
        return new DummyHandler();
      });
  EXPECT_TRUE(first);

  bool second = HandlerRegistry::RegisterHandler(
      "Foo",
      [](const std::string& /*loc*/,
         const std::unordered_map<std::string, std::string>& /*params*/) {
        return new DummyHandler();
      });
  EXPECT_FALSE(second);
}

// -----------------------------------------------------------------------------
// CreateHandler tests
// -----------------------------------------------------------------------------

// Ensure CreateHandler invokes the registered factory with correct arguments.
TEST(HandlerRegistryTest, CreateHandlerInvokesFactoryWithCorrectArgs) {
  std::string seenLocation;
  std::unordered_map<std::string, std::string> seenParams;

  ASSERT_TRUE(HandlerRegistry::RegisterHandler(
      "Recorder",
      [&](const std::string& loc,
          const std::unordered_map<std::string, std::string>& params) {
        seenLocation = loc;
        seenParams = params;
        return new DummyHandler();
      }));

  std::unordered_map<std::string, std::string> params = {
      {"root", "./files"},
      {"foo", "bar"}
  };

  RequestHandler* h = HandlerRegistry::CreateHandler("Recorder", "/test", params);
  ASSERT_NE(h, nullptr);
  delete h;

  EXPECT_EQ(seenLocation, "/test");
  EXPECT_EQ(seenParams.size(), 2u);
  EXPECT_EQ(seenParams.at("root"), "./files");
  EXPECT_EQ(seenParams.at("foo"), "bar");
}

// Looking up an unknown handler name should throw a runtime_error.
TEST(HandlerRegistryTest, CreateHandlerUnknownNameThrows) {
  EXPECT_THROW(
    HandlerRegistry::CreateHandler("DoesNotExist", "/", {}),
    std::runtime_error
  );
}

// -----------------------------------------------------------------------------
// Built-in handler registration
// -----------------------------------------------------------------------------

// EchoHandler should have auto-registered itself at load time.
TEST(HandlerRegistryTest, EchoHandlerIsRegistered) {
  EXPECT_TRUE(HandlerRegistry::HasHandlerFor(EchoHandler::kName));
}

// We should be able to create an EchoHandler via the registry.
TEST(HandlerRegistryTest, CreateEchoHandlerViaRegistry) {
  RequestHandler* raw =
    HandlerRegistry::CreateHandler(EchoHandler::kName, "/echo", {});
  ASSERT_NE(raw, nullptr);

  auto* eh = dynamic_cast<EchoHandler*>(raw);
  EXPECT_NE(eh, nullptr);
  delete raw;
}
// StaticHandler should have auto-registered itself at load time.
TEST(HandlerRegistryTest, StaticHandlerIsRegistered) {
  EXPECT_TRUE(HandlerRegistry::HasHandlerFor(StaticHandler::kName));
}

// We should be able to create a StaticHandler via the registry.
TEST(HandlerRegistryTest, CreateStaticHandlerViaRegistry) {
  std::unordered_map<std::string,std::string> params = {{"root","."}};
  RequestHandler* raw =
    HandlerRegistry::CreateHandler(StaticHandler::kName, "/static", params);
  ASSERT_NE(raw, nullptr);
  auto* sh = dynamic_cast<StaticHandler*>(raw);
  EXPECT_NE(sh, nullptr);
  delete raw;
}

// Each call to CreateHandler must return a new, distinct instance.
TEST(HandlerRegistryTest, CreateHandlerReturnsDistinctInstances) {
  auto* a = HandlerRegistry::CreateHandler(EchoHandler::kName, "/echo", {});
  auto* b = HandlerRegistry::CreateHandler(EchoHandler::kName, "/echo", {});
  EXPECT_NE(a, b);
  delete a;
  delete b;
}

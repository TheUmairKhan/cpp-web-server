// tests/handler_registry_tests.cc

#include "handler_registry.h"
#include "request_handler.h"   // brings in RequestHandler, Request, Response
#include <gtest/gtest.h>
#include <stdexcept>

//
// A concrete dummy so HandlerRegistry can construct one.
// We never actually call handle_request in these tests,
// so throwing is fine and avoids needing a valid Response ctor.
//
class DummyHandler : public RequestHandler {
 public:
  Response handle_request(const Request& /*req*/) override {
    throw std::logic_error("DummyHandler should not be used here");
  }
};

TEST(HandlerRegistryTest, RegisterReturnsTrueThenFalseOnDuplicate) {
  // First registration of "Foo" should succeed.
  bool first = HandlerRegistry::RegisterHandler(
      "Foo",
      [](const std::string& /*loc*/,
         const std::unordered_map<std::string, std::string>& /*params*/) {
        return new DummyHandler();
      });
  EXPECT_TRUE(first);

  // Second registration under the same name should be rejected.
  bool second = HandlerRegistry::RegisterHandler(
      "Foo",
      [](const std::string& /*loc*/,
         const std::unordered_map<std::string, std::string>& /*params*/) {
        return new DummyHandler();
      });
  EXPECT_FALSE(second);
}

TEST(HandlerRegistryTest, CreateHandlerInvokesFactoryWithCorrectArgs) {
  std::string seenLocation;
  std::unordered_map<std::string, std::string> seenParams;

  // Register a factory that records its inputs.
  ASSERT_TRUE(HandlerRegistry::RegisterHandler(
      "Recorder",
      [&](const std::string& loc,
          const std::unordered_map<std::string, std::string>& params) {
        seenLocation = loc;
        seenParams = params;
        return new DummyHandler();
      }));

  // Prepare some dummy params
  std::unordered_map<std::string, std::string> params = {
      {"root", "./files"},
      {"foo", "bar"}
  };

  // Create it
  RequestHandler* h = HandlerRegistry::CreateHandler("Recorder", "/test", params);
  ASSERT_NE(h, nullptr);
  delete h;

  // Verify factory saw exactly what we passed in
  EXPECT_EQ(seenLocation, "/test");
  EXPECT_EQ(seenParams.size(), 2u);
  EXPECT_EQ(seenParams.at("root"), "./files");
  EXPECT_EQ(seenParams.at("foo"), "bar");
}

TEST(HandlerRegistryTest, CreateHandlerUnknownNameThrows) {
  // Lookup of an unregistered name should throw
  EXPECT_THROW(
    HandlerRegistry::CreateHandler("DoesNotExist", "/", {}),
    std::runtime_error
  );
}

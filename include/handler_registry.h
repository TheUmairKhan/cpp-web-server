// handler_registry.h
#ifndef HANDLER_REGISTRY_H
#define HANDLER_REGISTRY_H

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "request_handler.h"

class HandlerRegistry {
public:
  // factory signature: (configured location,  param-map) → new handler
  using RequestHandlerFactory =
    std::function<RequestHandler*(const std::string& location,
                                  const std::unordered_map<std::string,std::string>& params)>;

  // register under the handler’s “kName”
  static bool RegisterHandler(const std::string& name, RequestHandlerFactory factory);

  // look up by name, invoke factory
  static RequestHandler* CreateHandler(const std::string& name,
                                       const std::string& location,
                                       const std::unordered_map<std::string,std::string>& params);

private:
  static std::unordered_map<std::string,RequestHandlerFactory>& registry();
};

// at static‐init time this will run and register the factory
#define REGISTER_HANDLER(HandlerClass)                              \
  namespace {                                                       \
    const bool _reg_##HandlerClass =                                \
      HandlerRegistry::RegisterHandler(                             \
        HandlerClass::kName,                                        \
        [](const std::string& loc,                                  \
           const std::unordered_map<std::string,std::string>& args) \
          { return HandlerClass::Init(loc, args); }                \
      );                                                            \
  }

#endif  // HANDLER_REGISTRY_H

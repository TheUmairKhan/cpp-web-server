// handler_registry.cc
#include "handler_registry.h"
#include <stdexcept>

std::unordered_map<std::string, HandlerRegistry::RequestHandlerFactory>&
HandlerRegistry::registry() {
  static std::unordered_map<std::string, RequestHandlerFactory> map;
  return map;
}

bool HandlerRegistry::RegisterHandler(const std::string& name,
                                      RequestHandlerFactory factory) {
  auto& m = registry();
  if (m.count(name)) return false;      // prevent duplicates
  m[name] = std::move(factory);
  return true;
}

RequestHandler* HandlerRegistry::CreateHandler(
    const std::string& name,
    const std::string& location,
    const std::unordered_map<std::string,std::string>& params) {
  auto& m = registry();
  auto it = m.find(name);
  if (it == m.end()) {
    throw std::runtime_error("Unknown handler: " + name);
  }
  return it->second(location, params);
}

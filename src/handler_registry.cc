#include "handler_registry.h"
#include <stdexcept>

// Returns the single registry map, created on first call
std::unordered_map<std::string, HandlerRegistry::RequestHandlerFactory>&
HandlerRegistry::registry() {
  static std::unordered_map<std::string, RequestHandlerFactory> map;
  return map;
}

// Store the factory under 'name'; skip if already exists.
bool HandlerRegistry::RegisterHandler(const std::string& name,
                                      RequestHandlerFactory factory) {
  auto& m = registry();
  if (m.count(name)) return false;  // duplicate registration not allowed
  m[name] = std::move(factory);
  return true;
}

// Look up 'name' and invoke its factory with the supplied args.
// Throws if no such handler was registered.
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

// Check if any factory is registered under this name.
bool HandlerRegistry::HasHandlerFor(const std::string& name) {
    return registry().count(name) > 0;
}
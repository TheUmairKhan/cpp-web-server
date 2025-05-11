// handler_registry.h
#ifndef HANDLER_REGISTRY_H
#define HANDLER_REGISTRY_H

#include <string>
#include <unordered_map>
#include <functional>
#include "request_handler.h"

class HandlerRegistry {
public:
    // Factory signature: (location prefix, parsed params) → new RequestHandler*
    using RequestHandlerFactory = 
        std::function<RequestHandler*(const std::string& /*location*/,
                                      const std::unordered_map<std::string, std::string>& /*params*/)>;

    // Register a factory under a unique name. Returns false if already present.
    static bool RegisterHandler(const std::string& name, RequestHandlerFactory factory);

    // Instantiate a handler by name. Throws std::runtime_error if unknown.
    static RequestHandler* CreateHandler(const std::string& name,
                                         const std::string& location,
                                         const std::unordered_map<std::string, std::string>& params);

    // Check if any factory is registered under this name.
    static bool HasHandlerFor(const std::string& name);

private:
    // Returns the singleton map of name→factory
    static std::unordered_map<std::string, RequestHandlerFactory>& registry();
};

#endif  // HANDLER_REGISTRY_H

#ifndef ROUTER_H
#define ROUTER_H

#include "request_handler.h"
#include "request.h"
#include "response.h"
#include "handler_registry.h"
#include <memory>
#include <vector>
#include <utility>
#include <unordered_map>

class Router {
public:
    using Factory = HandlerRegistry::RequestHandlerFactory;

    // Register a route: we store the factory + its params,
    // but do *not* instantiate anything yet.
    void add_route(const std::string& path_prefix,
                    Factory factory,
                    std::unordered_map<std::string,std::string> params);

    // Returns a vector of route paths
    std::vector<std::string> get_routes() const;
    
    // Given a request, passes it to its proper handler and returns the generated response
    Response handle_request(const Request& request) const;

private:
    struct RouteEntry {
        std::string prefix;
        Factory factory;
        std::unordered_map<std::string,std::string> params;
    };
    // Vector containing Router object's routes, where each entry is a pair of
    // (path string, handler)
    std::vector<RouteEntry> routes_;
    
    // Sanitizes and returns a given path, removing extraneous characters
    std::string sanitize_path(const std::string& path) const;
};

#endif // ROUTER_H
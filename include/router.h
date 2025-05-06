#ifndef ROUTER_H
#define ROUTER_H

#include "request_handler.h"
#include "request.h"
#include "response.h"
#include <memory>
#include <vector>
#include <utility>
#include <unordered_map>

class Router {
public:
    // Adds a route given a path and its handler
    void add_route(const std::string& path_prefix, 
                  std::unique_ptr<RequestHandler> handler);

    // Returns a vector of route paths
    std::vector<std::string> get_routes() const;
    
    // Given a request, passes it to its proper handler and returns the generated response
    Response handle_request(const Request& request) const;

private:
    // Vector containing Router object's routes, where each entry is a pair of
    // (path string, handler)
    std::vector<std::pair<std::string, std::unique_ptr<RequestHandler>>> routes_;
    
    // Sanitizes and returns a given path, removing extraneous characters
    std::string sanitize_path(const std::string& path) const;
};

#endif // ROUTER_H
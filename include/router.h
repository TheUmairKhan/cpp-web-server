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
    void add_route(const std::string& path_prefix, 
                  std::unique_ptr<RequestHandler> handler);

    std::vector<std::string> get_routes() const;
    
    Response handle_request(const Request& request) const;

private:
    std::vector<std::pair<std::string, std::unique_ptr<RequestHandler>>> routes_;
    
    std::string sanitize_path(const std::string& path) const;
};

#endif // ROUTER_H
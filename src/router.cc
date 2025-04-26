#include "router.h"
#include <algorithm>

void Router::add_route(const std::string& path_prefix, 
                      std::unique_ptr<RequestHandler> handler) {
    routes_.emplace_back(sanitize_path(path_prefix), std::move(handler));
}

Response Router::handle_request(const Request& request) const {
    const std::string path = sanitize_path(request.get_url());
    
    // Find longest matching prefix
    auto best_match = routes_.cend();
    size_t max_length = 0;
    
    for (auto it = routes_.cbegin(); it != routes_.cend(); ++it) {
        const std::string& prefix = it->first;
        if (path.compare(0, prefix.length(), prefix) == 0) {
            if (prefix.length() > max_length) {
                max_length = prefix.length();
                best_match = it;
            }
        }
    }
    
    if (best_match != routes_.cend()) {
        return best_match->second->handle_request(request);
    }
    
    // Default 404 handler
    return Response(request.get_version(), 404, "text/plain", 9, "close", "Not Found");
}

std::string Router::sanitize_path(const std::string& path) const {
    std::string sanitized = path;
    
    // Ensure path starts with /
    if (sanitized.empty() || sanitized[0] != '/') {
        sanitized = "/" + sanitized;
    }
    
    // Remove trailing slash
    if (sanitized.size() > 1 && sanitized.back() == '/') {
        sanitized.pop_back();
    }
    
    return sanitized;
}
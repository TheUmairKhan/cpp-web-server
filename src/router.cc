#include "router.h"
#include <algorithm>

void Router::add_route(const std::string& path_prefix,
                       Factory factory,
                       std::unordered_map<std::string,std::string> params) {
  routes_.push_back(RouteEntry{
    sanitize_path(path_prefix),
    std::move(factory),
    std::move(params)
  });
}

std::vector<std::string> Router::get_routes() const {
  std::vector<std::string> out;
  for (auto& e : routes_) out.push_back(e.prefix);
  return out;
}

Response Router::handle_request(const Request& request) const {
  const std::string path = sanitize_path(request.get_url());

  // find longest‐matching prefix
  const RouteEntry* best = nullptr;
  size_t best_len = 0;
  for (auto& e : routes_) {
    if (path.rfind(e.prefix, 0) == 0 && e.prefix.size() > best_len) {
      best = &e;
      best_len = e.prefix.size();
    }
  }

  //Expect the NotFoundHandler to be registered at '/'
  //If no match is found, we have a configuration error
  if (!best) {
    return Response(request.get_version(), 500, // Returns a 500 Internal Server Error in this case
                   "text/plain", 32, "close", 
                   "Server Error: No handlers registered");
  }

  // **per-request** instantiate, use, then destroy:
  RequestHandler* h = best->factory(best->prefix, best->params);
  Response resp = h->handle_request(request);
  delete h;
  return resp;
}

std::string Router::sanitize_path(const std::string& path) const {
  std::string s = path;
  if (s.empty() || s[0] != '/') s.insert(s.begin(), '/');
  if (s.size()>1 && s.back()=='/') s.pop_back();
  return s;
}

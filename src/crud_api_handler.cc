// src/static_handler.cc
#include "crud_api_handler.h"
#include <fstream>
#include <iterator>
#include <cstring>
#include <nlohmann/json.hpp>

// define the kName symbol
constexpr char CrudApiHandler::kName[];

using json = nlohmann::json;

// Factory invoked by HandlerRegistry
RequestHandler* CrudApiHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
  auto it = params.find("root");
  if (it == params.end()) {
    throw std::runtime_error(
      "CrudApiHandler missing 'root' parameter for location " + location);
  }

  // canonicalize the root on disk
  fs::path cfg = it->second;
  fs::path abs_root = cfg.is_absolute()
    ? fs::canonical(cfg)
    : fs::weakly_canonical(fs::read_symlink("/proc/self/exe").parent_path() / cfg);

  return new CrudApiHandler(location, abs_root.string());
}

// Constructor saves both pieces of information
CrudApiHandler::CrudApiHandler(std::string url_prefix, std::string filesystem_root)
  : prefix_(std::move(url_prefix)),
    fs_root_(std::move(filesystem_root)) {}

// Build the real filesystem path, guard against traversal
std::string CrudApiHandler::resolve_path(const std::string& url_path) const {
  // strip off the URL prefix
  if (url_path.rfind(prefix_,0)!=0) {
    throw std::runtime_error("No static mount for this path");
  }
  std::string rest = url_path.substr(prefix_.size());
  if (!rest.empty() && rest[0]=='/') rest.erase(0,1);

  // canonicalize base and candidate
  fs::path base = fs::canonical(fs_root_);
  fs::path full = fs::weakly_canonical(base / rest);

  // ensure full stays under base
  if (full.generic_string().rfind(base.generic_string(),0)!=0) {
    throw std::runtime_error("Path traversal attempt detected");
  }
  return full.string();
}

bool CrudApiHandler::is_valid_json(const std::string& body) const {
    try { 
      auto doc = json::parse(body); 
      return true; 
    }
    catch (const nlohmann::json::parse_error&) { 
      return false; 
    }
}

// The actual request handler
Response CrudApiHandler::handle_request(const Request& request) {
  auto method = request.get_method();

  if (method == "GET") {
    //TODO
  }
  else if (method == "POST") {
    //TODO
  }
  else if (method == "PUT") {
    //TODO
  }
  else if (method == "DELETE") {
    //TODO
  }

  std::string b = "500 Error: Handler Not Implemented";
      return Response(request.get_version(), 500,
                      "text/plain", b.size(), "close", b);
}

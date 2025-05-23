// src/static_handler.cc
#include "static_handler.h"
#include <fstream>
#include <iterator>
#include <cstring>

// define the kName symbol
constexpr char StaticHandler::kName[];

// Factory invoked by HandlerRegistry
RequestHandler* StaticHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
  auto it = params.find("root");
  if (it == params.end()) {
    throw std::runtime_error(
      "StaticHandler missing 'root' parameter for location " + location);
  }

  // canonicalize the root on disk
  fs::path cfg = it->second;
  fs::path abs_root = cfg.is_absolute()
    ? fs::canonical(cfg)
    : fs::weakly_canonical(fs::read_symlink("/proc/self/exe").parent_path() / cfg);

  return new StaticHandler(location, abs_root.string());
}

// Constructor saves both pieces of information
StaticHandler::StaticHandler(std::string url_prefix, std::string filesystem_root)
  : prefix_(std::move(url_prefix)),
    fs_root_(std::move(filesystem_root)) {}

// Extract file extension (including the dot), or "" if none
std::string StaticHandler::get_extension(const std::string& path) const {
  auto pos = path.find_last_of('.');
  return (pos == std::string::npos ? "" : path.substr(pos));
}

// A small static table; falls back to octet-stream
std::string StaticHandler::get_mime_type(const std::string& ext) const {
  static const std::unordered_map<std::string, std::string> m = {
    {".html","text/html"},{".htm","text/html"},{".txt","text/plain"},
    {".css","text/css"},{".js","application/javascript"},
    {".json","application/json"},{".jpg","image/jpeg"},
    {".jpeg","image/jpeg"},{".png","image/png"},{".gif","image/gif"},
    {".svg","image/svg+xml"},{".zip","application/zip"},
    {".pdf","application/pdf"}
  };
  auto it = m.find(ext);
  return it==m.end() ? "application/octet-stream" : it->second;
}

// Build the real filesystem path, guard against traversal
std::string StaticHandler::resolve_path(const std::string& url_path) const {
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

// The actual request handler
Response StaticHandler::handle_request(const Request& request) {
  try {
    auto path = resolve_path(request.get_url());
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      // 404 Not Found
      std::string b = "404 Error: File not found";
      return Response(request.get_version(), 404, "text/plain", b.size(), "close", b, StaticHandler::kName);
    }

    // slurp the file
    std::string body{ std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>() };

    auto ext = get_extension(path);
    auto mime = get_mime_type(ext);
    return Response(request.get_version(), 200, mime, body.size(), "close", body, StaticHandler::kName);
  }
  catch (const std::runtime_error& e) {
    // 403 Forbidden on traversal or bad mount
    std::string msg = e.what();
    return Response(request.get_version(), 403, "text/plain", msg.size(), "close", msg, StaticHandler::kName);
  }
}

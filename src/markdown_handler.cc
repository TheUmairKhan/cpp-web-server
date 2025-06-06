// src/markdown_handler.cc
#include "markdown_handler.h"
#include "logger.h"
#include <fstream>
#include <iterator>
#include <cstring>

// define the kName symbol
constexpr char MarkdownHandler::kName[];

// Factory invoked by HandlerRegistry
RequestHandler* MarkdownHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
  auto it = params.find("root");
  if (it == params.end()) {
    throw std::runtime_error(
      "MarkdownHandler missing 'root' parameter for location " + location);
  }

  // canonicalize the root on disk
  fs::path cfg = it->second;
  fs::path abs_root = cfg.is_absolute()
    ? fs::canonical(cfg)
    : fs::weakly_canonical(fs::read_symlink("/proc/self/exe").parent_path() / cfg);

  return new MarkdownHandler(location, abs_root.string());
}

// Constructor saves both pieces of information
MarkdownHandler::MarkdownHandler(std::string url_prefix, std::string filesystem_root)
  : prefix_(std::move(url_prefix)),
    fs_root_(std::move(filesystem_root)) {}

// Extract file extension (including the dot), or "" if none
std::string MarkdownHandler::get_extension(const std::string& path) const {
  auto pos = path.find_last_of('.');
  return (pos == std::string::npos ? "" : path.substr(pos));
}

// Build the real filesystem path, guard against traversal
std::string MarkdownHandler::resolve_path(const std::string& url_path) const {
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
Response MarkdownHandler::handle_request(const Request& request) {
  auto method = request.get_method();

  if (method == "GET") {
    return handle_get(request);
  }
  else if (method == "POST") {
    return handle_post(request);
  }
  
  // Unsupported method
  std::string b = "400 Bad Request: Unsupported method";
  Logger::log_error("MarkdownHandler error: " + b);
  return Response(request.get_version(), 400, "text/plain", b.size(), "close", b, MarkdownHandler::kName);
}

Response MarkdownHandler::handle_get(const Request& request) {
  try {
    auto path = resolve_path(request.get_url());
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      // 404 Not Found
      std::string b = "404: File not found";
      Logger::log_error("MarkdownHandler error: " + b);
      return Response(request.get_version(), 404, "text/plain", b.size(), "close", b, MarkdownHandler::kName);
    }

    // slurp the file
    std::string body{ std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>() };

    auto ext = get_extension(path);

    // 400 Bad Request if request file is not .md
    if (ext != ".md") {
        std::string msg = "400 Bad Request: Non-Markdown file requested";
        Logger::log_error("MarkdownHandler error: " + msg);
        return Response(request.get_version(), 400, "text/plain", msg.size(), "close", msg, MarkdownHandler::kName);
    }

    // Convert body from .md to .html
    std::string html_body = markdown::ConvertToHtml(body);
    std::string full_html = markdown::WrapInHtmlTemplate(html_body);

    return Response(request.get_version(), 200, "text/html; charset=utf-8", full_html.size(), "close", full_html, MarkdownHandler::kName);
  }
  catch (const std::runtime_error& e) {
    // 404 Not Found on traversal or bad mount to obscure file structure
    std::string msg = e.what();
    Logger::log_error("MarkdownHandler error: 404 Not Found: " + msg);
    return Response(request.get_version(), 404, "text/plain", msg.size(), "close", msg, MarkdownHandler::kName);
  }
}

Response MarkdownHandler::handle_post(const Request& request) {
  try {
    // ensure that content type is markdown
    auto content_type = request.get_header("Content-Type");
    if (content_type != "text/markdown") {
      std::string msg = "400 Bad Request: Post received non-Markdown content";
      Logger::log_error("MarkdownHandler error: " + msg);
      return Response(request.get_version(), 400, "text/plain", msg.size(), "close", msg, MarkdownHandler::kName);
    }

    // request body is markdown
    auto body = request.get_body();

    // Convert body from .md to .html
    std::string html_body = markdown::ConvertToHtml(body);
    std::string full_html = markdown::WrapInHtmlTemplate(html_body);

    return Response(request.get_version(), 200, "text/html; charset=utf-8", full_html.size(), "close", full_html, MarkdownHandler::kName);
  }
  catch (const std::runtime_error& e) {
    // 404 Not Found on traversal or bad mount to obscure file structure
    std::string msg = e.what();
    Logger::log_error("MarkdownHandler error: 404 Not Found: " + msg);
    return Response(request.get_version(), 404, "text/plain", msg.size(), "close", msg, MarkdownHandler::kName);
  }
}

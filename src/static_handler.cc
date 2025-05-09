#include "static_handler.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

std::unordered_map<std::string, std::string> StaticHandler::prefix_to_path_;

void StaticHandler::configure(const std::string& url_prefix,
                              const std::string& filesystem_path) {
    fs::path cfg(filesystem_path);

    // If the config path is absolute, use it directly; otherwise
    // anchor a relative path under the folder holding our binary.
    fs::path abs_root;
    if (cfg.is_absolute()) {
        abs_root = fs::canonical(cfg);
    } else {
        // 1) where the running exe lives
        fs::path exe    = fs::read_symlink("/proc/self/exe");
        fs::path bindir = exe.parent_path();
        // 2) resolve bindir + relative cfg
        abs_root = fs::weakly_canonical(bindir / cfg);
    }

    // Create it if it doesn’t exist
    fs::create_directories(abs_root);

    // Finally store it for serve-time lookup
    prefix_to_path_[url_prefix] = abs_root.string();
}

std::string StaticHandler::get_extension(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) return "";
    return path.substr(dot_pos);
}

std::string StaticHandler::get_mime_type(const std::string& file_extension) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".txt", "text/plain"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".zip", "application/zip"},
        {".pdf", "application/pdf"}
    };
    
    auto it = mime_types.find(file_extension);
    return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

std::string StaticHandler::resolve_path(const std::string& url_path)
{
    /* ───────────────────────────────
       1.  Find the *longest* matching
           mount-point (/static, /public …)
    ─────────────────────────────── */
    std::string matched_prefix;
    std::string remaining;

    for (const auto& [prefix, fs_root] : prefix_to_path_) {
        if (url_path.rfind(prefix, 0) == 0 &&           // prefix matches start
            prefix.size() > matched_prefix.size()) {    // keep the longest
            matched_prefix = prefix;
            remaining      = url_path.substr(prefix.size());
        }
    }

    if (matched_prefix.empty())
        throw std::runtime_error("No static handler configured for this path");

    /* ───────────────────────────────
       2.  Build the tentative path
    ─────────────────────────────── */
    if (!remaining.empty() && remaining.front() == '/')
        remaining.erase(0, 1);                          // drop the leading '/'

    fs::path base  = fs::canonical(prefix_to_path_[matched_prefix]);
    fs::path full  = fs::weakly_canonical(base / remaining);

    /* ───────────────────────────────
       3.  Path-traversal defence
    ─────────────────────────────── */
    const std::string base_str = base.generic_string();
    const std::string full_str = full.generic_string();

    if (full_str.rfind(base_str, 0) != 0)               // not a prefix
        throw std::runtime_error("Path traversal attempt detected");

    return full_str;
}

Response StaticHandler::handle_request(const Request& request) {
    try {
        std::string fs_path = resolve_path(request.get_url());
        
        std::ifstream file(fs_path, std::ios::binary);
        if (!file) {
            const std::string body = "404 Error: File not found";
            return Response(request.get_version(), 404, "text/plain", 
                           body.size(), "close", body);
        }
        
        std::string content_type = get_mime_type(get_extension(fs_path));
        std::string body((std::istreambuf_iterator<char>(file)), 
                        std::istreambuf_iterator<char>());
        
        return Response(request.get_version(), 200, content_type,
                       body.size(), "close", body);
    }
    catch (const std::runtime_error& e) {
        return Response(request.get_version(), 403, "text/plain",
                       strlen(e.what()), "close", e.what());
    }
}
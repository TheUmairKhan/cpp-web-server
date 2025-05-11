#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include <string>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

class StaticHandler : public RequestHandler {
public:
  // The key that must appear in your config
  static constexpr char kName[] = "StaticHandler";

  // Called by the registry to produce a configured instance.
  //  - location is the URL prefix (e.g. "/static")
  //  - params["root"] is the directory on disk
  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);


  Response handle_request(const Request& request) override;

private:
  // Each handler instance needs exactly these two pieces of information:
  StaticHandler(std::string url_prefix, std::string filesystem_root);

  // The mount point (prefix) we were configured with.
  std::string prefix_;
  // The absolute filesystem root we were configured with.
  std::string fs_root_;

  // helpers
  std::string get_extension(const std::string& path) const;
  std::string get_mime_type(const std::string& ext) const;
  std::string resolve_path(const std::string& url_path) const;
};

// one-time registration at load time:
inline bool _static_handler_registered =
    HandlerRegistry::RegisterHandler(
        StaticHandler::kName,
        StaticHandler::Init);

#endif  // STATIC_HANDLER_H

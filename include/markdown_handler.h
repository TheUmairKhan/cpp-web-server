#ifndef MARKDOWN_HANDLER_H
#define MARKDOWN_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include "markdown_converter.h"
#include <string>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

class MarkdownHandler : public RequestHandler {
public:
  // The key that must appear in your config
  static constexpr char kName[] = "MarkdownHandler";

  // Called by the registry to produce a configured instance.
  //  - location is the URL prefix (e.g. "/docs")
  //  - params["root"] is the directory on disk
  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);


  Response handle_request(const Request& request) override;

private:
  // Each handler instance needs exactly these two pieces of information:
  MarkdownHandler(std::string url_prefix, std::string filesystem_root);

  // The mount point (prefix) we were configured with.
  std::string prefix_;
  // The absolute filesystem root we were configured with.
  std::string fs_root_;

  // helpers
  std::string get_extension(const std::string& path) const;
  std::string resolve_path(const std::string& url_path) const;
};

// one-time registration at load time:
inline bool _markdown_handler_registered =
    HandlerRegistry::RegisterHandler(
        MarkdownHandler::kName,
        MarkdownHandler::Init);

#endif  // MARKDOWN_HANDLER_H

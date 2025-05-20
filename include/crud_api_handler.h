#ifndef CRUD_API_HANDLER_H
#define CRUD_API_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include "filesystem.h"
#include "real_filesystem.h"
#include <string>
#include <filesystem>
#include <stdexcept>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace fs = std::filesystem;
namespace pt = boost::property_tree;

class CrudApiHandler : public RequestHandler {
public:
  // The key that must appear in your config
  static constexpr char kName[] = "CrudApiHandler";

  // Called by the registry to produce a configured instance.
  //  - location is the URL prefix (e.g. "/static")
  //  - params["root"] is the directory on disk
  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);

  // Each handler instance needs exactly these two pieces of information:
  CrudApiHandler(std::string url_prefix, std::string filesystem_root,
                std::shared_ptr<FileSystemInterface> fs = std::make_shared<RealFileSystem>());

  Response handle_request(const Request& request) override;

private:
  // The mount point (prefix) we were configured with.
  std::string prefix_;
  // The absolute filesystem root we were configured with.
  std::string fs_root_;
  // The filesystem implementation to use
  std::shared_ptr<FileSystemInterface> fs_impl_;

  std::string entity_;
  int entity_id_;

  // helpers
  bool is_valid_json(const std::string& body) const;
  std::string parse_for_entity(const std::string& url_path) const;
  std::string parse_for_id(const std::string& url_path) const;
  int generate_unique_id(const std::string& entity_type) const;

  Response make_error_response(const Request& request, int status_code, const std::string& message) const;
  Response make_success_response(const Request& request, const std::string& response_type, const std::string& message) const;

  Response handle_post(const Request& request, const std::string& entity_type);
  Response handle_get(const Request& request, const std::string& entity_type, const std::string& entity_id);
  Response handle_put(const Request& request, const std::string& entity_type, const std::string& entity_id);
  Response handle_delete(const Request& request, const std::string& entity_type, const std::string& entity_id);
};

// one-time registration at load time:
inline bool _crud_api_handler_registered =
    HandlerRegistry::RegisterHandler(
        CrudApiHandler::kName,
        CrudApiHandler::Init);

#endif  // CRUD_API_HANDLER
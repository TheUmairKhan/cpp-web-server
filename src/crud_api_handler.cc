// src/static_handler.cc
#include "crud_api_handler.h"
#include <fstream>
#include <iterator>
#include <cstring>

// define the kName symbol
constexpr char CrudApiHandler::kName[];

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
    std::stringstream ss(body);
    pt::ptree pt;
    pt::read_json(ss, pt);
    return true;
  } 
  catch (const pt::ptree_error& e) {
    return false;
  } 
  catch (const std::exception& e) {
    return false;
  }
}

std::string CrudApiHandler::parse_for_entity(const std::string& url_path) const {
  if (url_path.rfind(prefix_, 0) != 0) {
    return "";
  }
  
  std::string rest = url_path.substr(prefix_.size());
  if (!rest.empty() && rest[0] == '/') rest.erase(0, 1);

  size_t slash_pos = rest.find('/');
  if (slash_pos == std::string::npos) { //no id present
    return rest;
  } else { //id is present
    return rest.substr(0, slash_pos);
  }
}

std::string CrudApiHandler::parse_for_id(const std::string& url_path) const {
  if (url_path.rfind(prefix_, 0) != 0) {
        return "";
  }

  std::string rest = url_path.substr(prefix_.size());
  if (!rest.empty() && rest[0] == '/') rest.erase(0, 1);

  // Find the first '/' after the prefix
  size_t slash_pos = rest.find('/');
  if (slash_pos == std::string::npos) { //no id present
    return "";
  } else {
    std::string id_str = rest.substr(slash_pos + 1);
    return id_str;
  }
}

int CrudApiHandler::generate_unique_id(const std::string& entity) const {
  fs::path entity_dir_path = fs::path(fs_root_) / entity;
  int max_id = 0;

  if (fs::exists(entity_dir_path) && fs::is_directory(entity_dir_path)) {
      for (const auto& entry : fs::directory_iterator(entity_dir_path)) {
           if (entry.is_regular_file()) {
               try {
                   // Attempt to convert filename to integer ID
                   int file_id = std::stoi(entry.path().filename().string());
                   max_id = std::max(max_id, file_id);
               } catch (const std::invalid_argument& e) {
                   // Ignore files that are not valid integers
               } catch (const std::out_of_range& e) {
                   // Ignore files with IDs out of integer range
               }
           }
       }
   }

   return max_id + 1;
   //The ID returned is never less than any ID in the file directory
}

Response CrudApiHandler::handle_post(const Request& request, const std::string& entity_type){
  //verify request body is valid json
    std::string request_body = request.get_body();
    if (!is_valid_json(request_body)) {
        std::string b = "400 Bad Request: Invalid JSON in request body";
        return Response(request.get_version(), 400, "text/plain", b.size(), "close", b);
    }

    //Check if directory exists, create if not
    fs::path entity_dir_path = fs::path(fs_root_) / entity_type;
    try {
        if (!fs::exists(entity_dir_path)) {
            if (!fs::create_directories(entity_dir_path)) {
                 std::string b = "500 Internal Server Error: Could not create entity directory";
                 return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
            }
        } else if (!fs::is_directory(entity_dir_path)) {
             std::string b = "500 Internal Server Error: Entity path is not a directory";
             return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
        }
    } catch (const fs::filesystem_error& e) {
         std::string b = "500 Internal Server Error: Filesystem error creating directory";
         return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
    }

    int new_id = generate_unique_id(entity_type);

    fs::path file_path = entity_dir_path / std::to_string(new_id);

    //write json body into file system
    try {
        std::ofstream ofs(file_path);
        if (!ofs.is_open()) {
             std::string b = "500 Internal Server Error: Could not open file for writing";
             return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
        }
        ofs << request_body;
        ofs.close();
    } catch (const std::exception& e) {
         std::string b = "500 Internal Server Error: Error writing to file";
         return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
    }

    //return json with id of newly created entity object
    pt::ptree response_body_pt;
    response_body_pt.put("id", new_id);

    std::stringstream ss;
    pt::write_json(ss, response_body_pt);
    std::string response_body_str = ss.str();

    return Response(request.get_version(), 200, "application/json", response_body_str.size(), "close", response_body_str);
}

// The actual request handler
Response CrudApiHandler::handle_request(const Request& request) {
  auto method = request.get_method();

  //get entity from web url and return errors if it doesn't exist
  std::string entity_type;
  try {
      entity_type = parse_for_entity(request.get_url());
      if (entity_type.empty()) {
            std::string b = "400 Bad Request: Missing entity type in URL";
            return Response(request.get_version(), 400, "text/plain", b.size(), "close", b);
      }
  } catch (const std::exception& e) {
      // Handle potential errors in parse_for_entity
      std::string b = "400 Bad Request: Invalid URL format";
      return Response(request.get_version(), 400, "text/plain", b.size(), "close", b);
  }

  //get ID similar to parsing entity
  std::string entity_id;
  try {
    entity_id = parse_for_id(request.get_url());
  }
  catch (const std::exception& e) {
    std::string b = "400 Bad Request: Invalid URL format for ID";
    return Response(request.get_version(), 400, "text/plain", b.size(), "close", b);
  }


  if (method == "GET") {
    //TODO
  }
  else if (method == "POST") {
    return handle_post(request, entity_type);
  }
  else if (method == "PUT") {
    //TODO
  }
  else if (method == "DELETE") {
    //TODO
  }

  std::string b = "500 Error: Handler Not Implemented";
  return Response(request.get_version(), 500, "text/plain", b.size(), "close", b);
}

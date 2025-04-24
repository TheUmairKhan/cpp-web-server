#include "static_handler.h"
#include <fstream>
#include <sstream>

const std::string StaticHandler::base_dir_ = "../base_directory";

Response StaticHandler::handle_request(const Request& request) {
  std::string file_path = base_dir_ + request.get_url();
  std::ifstream file(file_path);
  if (!file) {
    std::string body = "File not found";
    return Response(request.get_version(), 404, "text/plain", body.length(), "close", body);
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string body = buffer.str();
  return Response(request.get_version(), 200, "text/html", body.length(), "close", body);
}

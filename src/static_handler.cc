#include "static_handler.h"

Response StaticHandler::handle_request(const Request& request) {
  std::string body = "Bad Request";
  return Response(request.get_version(), 400, "text/plain", body.length(), "close", body);
}
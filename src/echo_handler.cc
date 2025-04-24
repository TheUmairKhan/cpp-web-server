#include "echo_handler.h"

Response EchoHandler::handle_request(const Request& request) {
  // 400 Bad request if invalid request or not GET (others not implemented yet)
  if (!request.is_valid() || request.get_method() != "GET") {
    std::string body = "Bad Request";
    return Response(request.get_version(), 400, "text/plain", body.length(), "close", body);
  }

  // ---------- normal 200 echo path ----------
  return Response(request.get_version(), 200, "text/plain", request.length(), "close", request.to_string());
}
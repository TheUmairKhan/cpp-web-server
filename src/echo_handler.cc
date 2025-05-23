#include "echo_handler.h"

// define the kName symbol
constexpr char EchoHandler::kName[];

RequestHandler* EchoHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& /*params*/) {
  return new EchoHandler(location);
}

EchoHandler::EchoHandler(std::string location)
  : prefix_(std::move(location)) {}

Response EchoHandler::handle_request(const Request& request) {
  // 400 Bad request if method not supported
  if ((request.get_method() != "GET" && request.get_method() != "HEAD")) {
    std::string body = "Bad Request";
    return Response(request.get_version(), 400, "text/plain", body.length(), "close", body, EchoHandler::kName);
  }

  // ---------- normal 200 echo path ----------
  return Response(request.get_version(), 200, "text/plain", request.length(), "close", request.to_string(), EchoHandler::kName);
}
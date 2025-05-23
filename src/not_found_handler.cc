#include "not_found_handler.h"
#include "handler_registry.h"

// define the kName symbol
constexpr char NotFoundHandler::kName[];

// Factory method called by HandlerRegistry
RequestHandler* NotFoundHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& /*params*/) {
  return new NotFoundHandler(location);
}

// Constructor
NotFoundHandler::NotFoundHandler(std::string location)
  : prefix_(std::move(location)) {}

// Handles all requests with a 404 Not Found response
Response NotFoundHandler::handle_request(const Request& request) {
  //Create a standard 404 response
  std::string body = "404 Not Found: The requested resource could not be found on this server.";
  return Response(request.get_version(), 404, "text/plain", body.length(), "close", body, NotFoundHandler::kName);
}
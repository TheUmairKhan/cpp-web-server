#include "health_handler.h"
#include "handler_registry.h"

// define the kName symbol
constexpr char HealthHandler::kName[];

// Factory method called by HandlerRegistry
RequestHandler* HealthHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& /*params*/) {
  return new HealthHandler(location);
}

// Constructor
HealthHandler::HealthHandler(std::string location)
  : prefix_(std::move(location)) {}

// Always returns a 200 OK response
Response HealthHandler::handle_request(const Request& request) {
  std::string body = "OK";
  return Response(request.get_version(), 200, "text/plain", body.length(), "close", body);
}
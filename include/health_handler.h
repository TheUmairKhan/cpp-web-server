
#ifndef HEALTH_HANDLER_H
#define HEALTH_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include <string>
#include <unordered_map>

class HealthHandler : public RequestHandler {
 public:
   //Name used for registration in the factory
  static constexpr char kName[] = "HealthHandler";

  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);

  Response handle_request(const Request& request) override;

 private:
  explicit HealthHandler(std::string location);
  std::string prefix_;
};

//One-time registration at load time:
inline bool _health_handler_registered =
    HandlerRegistry::RegisterHandler(HealthHandler::kName,
                                     HealthHandler::Init);

#endif // HEALTH_HANDLER_H
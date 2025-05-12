
#ifndef NOT_FOUND_HANDLER_H
#define NOT_FOUND_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include <string>
#include <unordered_map>

class NotFoundHandler : public RequestHandler {
 public:
   //Name used for registration in the factory
  static constexpr char kName[] = "NotFoundHandler";

  static RequestHandler* Init(
      const std::string& location,
      const std::unordered_map<std::string, std::string>& params);

  NotFoundHandler(std::string location);

  Response handle_request(const Request& request) override;

 private:
  std::string prefix_;
};

//One-time registration at load time:
inline bool _not_found_handler_registered =
    HandlerRegistry::RegisterHandler(NotFoundHandler::kName,
                                     NotFoundHandler::Init);

#endif // NOT_FOUND_HANDLER_H
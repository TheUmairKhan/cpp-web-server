// echo_handler.h
#ifndef ECHO_HANDLER_H
#define ECHO_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"

class EchoHandler : public RequestHandler {
public:
    // The key this handler registers under
    static constexpr char kName[] = "EchoHandler";

    // Factory for dynamic instantiation
    static RequestHandler* Init(
        const std::string& location,
        const std::unordered_map<std::string, std::string>& params);

    // Serve incoming requests
    Response handle_request(const Request& request) override;

private:
    explicit EchoHandler(std::string location);
    std::string prefix_;
};

// Auto-register with the registry at static init time
inline bool _echo_registered =
    HandlerRegistry::RegisterHandler(EchoHandler::kName,
                                     EchoHandler::Init);

#endif  // ECHO_HANDLER_H

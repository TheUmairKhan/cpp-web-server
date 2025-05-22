#ifndef SLEEP_HANDLER_H
#define SLEEP_HANDLER_H

#include "request_handler.h"
#include "handler_registry.h"
#include <string>
#include <unordered_map>

class SleepHandler : public RequestHandler {
public:
    static constexpr char kName[] = "SleepHandler";
    
    static RequestHandler* Init(
        const std::string& location,
        const std::unordered_map<std::string, std::string>& params);
    
    explicit SleepHandler(std::string location, int sleep_seconds = 5);
    
    Response handle_request(const Request& request) override;

private:
    std::string prefix_;
    int sleep_duration_;
};

// Registration at init time
inline bool _sleep_handler_registered =
    HandlerRegistry::RegisterHandler(SleepHandler::kName,
                                     SleepHandler::Init);

#endif  // SLEEP_HANDLER_H
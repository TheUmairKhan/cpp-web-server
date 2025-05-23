#include "sleep_handler.h"
#include "logger.h"
#include <thread>
#include <chrono>

// define the kName symbol
constexpr char SleepHandler::kName[];

// Factory method called by HandlerRegistry
RequestHandler* SleepHandler::Init(
    const std::string& location,
    const std::unordered_map<std::string, std::string>& params) {
    
    int sleep_seconds = 5; // default
    auto it = params.find("sleep_duration");
    if (it != params.end()) {
        try {
            sleep_seconds = std::stoi(it->second);
        } catch (...) {
            //Parsing fails? Use default
        }
    }
    
    return new SleepHandler(location, sleep_seconds);
}

// Constructor
SleepHandler::SleepHandler(std::string location, int sleep_seconds)
    : prefix_(std::move(location)), sleep_duration_(std::max(0, sleep_seconds)) {}

// Handles requests by sleeping for the configured duration
Response SleepHandler::handle_request(const Request& request) {
    Logger::log_info("SleepHandler: Starting sleep for " + std::to_string(sleep_duration_) + " seconds");
    
    // Sleep for the configured duration
    std::this_thread::sleep_for(std::chrono::seconds(sleep_duration_));
    
    Logger::log_info("SleepHandler: Finished sleeping");
    
    // Returns response
    std::string body = "Slept for " + std::to_string(sleep_duration_) + " seconds";
    return Response(request.get_version(), 200, "text/plain", body.length(), "close", body, SleepHandler::kName);
}
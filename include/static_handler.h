#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "request_handler.h"
#include <string>
#include <unordered_map>
#include <filesystem>
#include <cstring>

class StaticHandler : public RequestHandler {
public:
    static void configure(const std::string& url_prefix, 
                         const std::string& filesystem_path);
    
    Response handle_request(const Request& request) override;

private:
    static std::unordered_map<std::string, std::string> prefix_to_path_;
    
    static std::string get_mime_type(const std::string& file_extension);
    static std::string resolve_path(const std::string& url_path);
    static std::string get_extension(const std::string& path);
};

#endif // STATIC_HANDLER_H
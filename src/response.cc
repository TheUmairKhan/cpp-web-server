#include "response.h"

Response::Response(const std::string& version, 
                   int status_code,
                   std::string content_type,
                   int content_length,
                   std::string connection,
                   std::string body) :
                   status_code_(status_code),
                   content_type_(content_type),
                   content_length_(content_length),
                   connection_(connection),
                   body_(body)
{
    status_line_ = version + " " + status_messages_.at(status_code);
}

std::string Response::to_string() {
    std::string response = status_line_ + "\r\n";
    response += "Content-Type: " + content_type_ + "\r\n";
    response += "Content-Length: " + std::to_string(content_length_) + "\r\n";
    response += "Connection: " + connection_ + "\r\n\r\n";
    response += body_;
    return response;
}

int Response::get_status_code() const { return status_code_; }

const std::unordered_map<int, std::string> Response::status_messages_ = {
    {200, "200 OK"},
    {400, "400 Bad Request"},
    {403, "403 Forbidden"},
    {404, "404 Not Found"},
    {500, "500 Internal Server Error"}
};
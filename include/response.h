#ifndef RESPONSE_H
#define RESPONSE_H

#include <string>
#include <unordered_map>

class Response {
  public:
    explicit Response(const std::string& version, 
                      int status_code,
                      std::string content_type,
                      int content_length,
                      std::string connection,
                      std::string body);

    // Returns string of response
    std::string to_string();

  private:  
    std::string status_line_;
    std::string content_type_;
    int content_length_;
    std::string connection_;
    std::string body_;
    static const std::unordered_map<int, std::string> status_messages_;
};

#endif 
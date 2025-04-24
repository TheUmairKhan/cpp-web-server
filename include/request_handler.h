#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <string>

class RequestHandler {
  public:
    virtual ~RequestHandler() {}

    virtual void handle_request(const std::string& request, std::string& response) = 0;
};
    
#endif // REQUEST_HANDLER_H
#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <request.h>
#include <string>

class RequestHandler {
  public:
    virtual ~RequestHandler() {}

    virtual void handle_request(const Request& request, std::string& response) = 0;
};
    
#endif // REQUEST_HANDLER_H
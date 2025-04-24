#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <request.h>
#include <response.h>
#include <string>

class RequestHandler {
  public:
    virtual ~RequestHandler() {}

    virtual Response handle_request(const Request& request) = 0;
};
    
#endif // REQUEST_HANDLER_H
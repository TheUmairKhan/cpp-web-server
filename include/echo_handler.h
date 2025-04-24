#ifndef ECHO_HANDLER_H
#define ECHO_HANDLER_H

#include "request_handler.h"

class EchoHandler : public RequestHandler {
  public:
    Response handle_request(const Request& request) override;
};
    
#endif // ECHO_HANDLER_H
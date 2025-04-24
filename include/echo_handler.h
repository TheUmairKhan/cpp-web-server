#ifndef ECHO_HANDLER_H
#define ECHO_HANDLER_H

#include "request_handler.h"

class EchoHandler : public RequestHandler {
  public:
    void handle_request(const std::string& request, std::string& response) override;
};
    
#endif // ECHO_HANDLER_H
#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "request_handler.h"

class StaticHandler : public RequestHandler {
  public:
    void handle_request(const Request& request, std::string& response) override;
};
    
#endif // STATIC_HANDLER_H
#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "request_handler.h"

class StaticHandler : public RequestHandler {
  public:
    Response handle_request(const Request& request) override;
};
    
#endif // STATIC_HANDLER_H
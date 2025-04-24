#ifndef STATIC_HANDLER_H
#define STATIC_HANDLER_H

#include "request_handler.h"

class StaticHandler : public RequestHandler {
  public:
    Response handle_request(const Request& request) override;
  private:
    static const std::string base_dir_;
};
    
#endif // STATIC_HANDLER_H
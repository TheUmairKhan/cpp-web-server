#ifndef HTTP_RESPONDER_H
#define HTTP_RESPONDER_H

#include <string>

class HttpResponder {
  public:
    // HTTP helpers
    static bool request_complete(const std::string& in_buf_); // have we seen "\r\n\r\n" ?
    static void make_response(const std::string& in_buf_, std::string& out_buf_); // build HTTP/1.1 200 OK echo  
};

#endif // HTTP_RESPONDER_H
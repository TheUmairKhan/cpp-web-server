#include "echo_handler.h"

void EchoHandler::handle_request(const Request& request, std::string& response) {
  if (!request.is_valid()) {
    response  = "HTTP/1.1 400 Bad Request\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n\r\n";
    return;
  }

  bool is_get = request.get_method() == "GET";

  response.clear();

  if (!is_get) {   // ---------- 400 path ----------
    response  = "HTTP/1.1 400 Bad Request\r\n";
    response += "Content-Length: 0\r\n";
    response += "Connection: close\r\n\r\n";
    return;
  }

  // ---------- normal 200 echo path ----------
  //const std::string& body = in_buf_;
  response  = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: text/plain\r\n";
  response += "Content-Length: " + std::to_string(request.length()) + "\r\n";
  response += "Connection: close\r\n\r\n";
  response += request.to_string();
}
#include "echo_handler.h"

void EchoHandler::handle_request(const std::string& request, std::string& response) {
  // Grab the first line (up to CR/LF or LF) to inspect the method
  auto eol = request.find("\r\n");
  if (eol == std::string::npos) eol = request.find('\n');
  std::string first_line = request.substr(0, eol);

  bool is_get = first_line.rfind("GET ", 0) == 0;   // starts with "GET "

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
  response += "Content-Length: " + std::to_string(request.size()) + "\r\n";
  response += "Connection: close\r\n\r\n";
  response += request;
}
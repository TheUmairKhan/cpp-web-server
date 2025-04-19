#include "http_responder.h"

bool HttpResponder::request_complete(const std::string& in_buf_) {
  // Simple heuristic: headers end with a blank line (\r\n\r\n).
  // Added || for \n\n termination for the netcat terminal, since their newline doesn't produce \r\n but \n instead.
  return in_buf_.find("\r\n\r\n") != std::string::npos || in_buf_.find("\n\n") != std::string::npos;
}

void HttpResponder::make_response(const std::string& in_buf_, std::string& out_buf_) {
  // Grab the first line (up to CR/LF or LF) to inspect the method
  auto eol = in_buf_.find("\r\n");
  if (eol == std::string::npos) eol = in_buf_.find('\n');
  std::string first_line = in_buf_.substr(0, eol);

  bool is_get = first_line.rfind("GET ", 0) == 0;   // starts with "GET "

  out_buf_.clear();

  if (!is_get) {   // ---------- 400 path ----------
    out_buf_  = "HTTP/1.1 400 Bad Request\r\n";
    out_buf_ += "Content-Length: 0\r\n";
    out_buf_ += "Connection: close\r\n\r\n";
    return;
  }

  // ---------- normal 200 echo path ----------
  //const std::string& body = in_buf_;
  out_buf_  = "HTTP/1.1 200 OK\r\n";
  out_buf_ += "Content-Type: text/plain\r\n";
  out_buf_ += "Content-Length: " + std::to_string(in_buf_.size()) + "\r\n";
  out_buf_ += "Connection: close\r\n\r\n";
  out_buf_ += in_buf_;
}
#include "request.h"

#include <boost/algorithm/string.hpp>
#include <vector>

bool request_complete(const std::string& in_buf) {
  // Simple heuristic: headers end with a blank line (\r\n\r\n).
  // Added || for \n\n termination for the netcat terminal, since their newline doesn't produce \r\n but \n instead.
  return in_buf.find("\r\n\r\n") != std::string::npos || in_buf.find("\n\n") != std::string::npos;
}

static void getLine(std::string& request, std::string& line) {
    // Find first instance of \n 
    auto eol= request.find("\n");
    line = request.substr(0, eol);
    // Don't include \r if line ends with \r\n
    if (line.back() == '\r') line.pop_back();
    // Remove line from request
    request = request.substr(eol + 1);
}

Request::Request(const std::string& request) {
  // Grab the request line (up to CR/LF or LF)
  std::string req = request;
  std::string line;
  getLine(req, line);

  // Split the request line by space 
  std::vector<std::string> line_tokens;
  boost::split(line_tokens, line, boost::is_any_of(" "));
  if (line_tokens.size() != 3) { valid_request_ = false; return; }
  method_ = line_tokens[0];
  url_ = line_tokens[1];
  http_version_ = line_tokens[2]; 

  // For remaining lines, map headers to their values
  // If end of request, only remaining part will be \r\n or \n
  while (req != "\r\n" && req != "\n" && !req.empty()) {
    getLine(req, line);
    auto split = line.find(":");
    if (split == std::string::npos) { valid_request_ = false; return; }
    std::string header_name = line.substr(0, split);
    // if header value is empty after colon, i.e "" or " ", value set to empty string
    if (split + 2 >= line.length()) { headers[header_name] = ""; continue; }
    headers[header_name] = line.substr(split + 2); // Don't include space following colon
  }
  raw_text_ = request;
  length_ = request.length();
  valid_request_ = true;
}

std::string Request::get_method() const { return method_; }

std::string Request::get_url() const { return url_; }

std::string Request::get_version() const { return http_version_; }

std::string Request::get_header(const std::string& header_name) const { 
    auto it = headers.find(header_name);
    if (it == headers.end()) return "";
    return it->second; 
}

std::string Request::to_string() const { return raw_text_; }

bool Request::is_valid() const { return valid_request_; }

int Request::length() const { return length_; }



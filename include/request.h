#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <unordered_map>

// Helper function to determine whether collected string is a complete request
bool request_complete(const std::string& in_buf);

// Helper function to get next line of a request string
// Line is stored in line and remaining request in request
static void getLine(std::string& request, std::string& line);

class Request {
  public:
    explicit Request(const std::string& request);

    // method getter
    std::string get_method() const;

    // url getter
    std::string get_url() const;

    // version getter
    std::string get_version() const;

    // header getter
    std::string get_header(const std::string& header_name) const;

    // Returns string representation of request
    std::string to_string() const;

    // Returns true if valid request, false if not
    bool is_valid() const;

    // Returns length of the request
    int length() const;

  private:  
    std::string method_;
    std::string url_;
    std::string http_version_;
    std::unordered_map<std::string, std::string> headers;
    std::string raw_text_;
    bool valid_request_;
    int length_;
};

#endif 
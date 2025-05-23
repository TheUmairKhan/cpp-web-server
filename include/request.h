#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <unordered_map>

// Helper function to get next line of a request string
// Line is stored in line and remaining request in request
static void getLine(std::string& request, std::string& line);

static bool isValidMethod(const std::string& method);

static bool isValidVersion(const std::string& version);

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

    // body getter
    std::string get_body() const;

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
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    std::string raw_text_;
    bool valid_request_;
    int length_;
};

#endif 
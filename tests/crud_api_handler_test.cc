#include <gtest/gtest.h>
#include "crud_api_handler.h"
#include "request.h"
#include "response.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace fs = std::filesystem;
namespace pt = boost::property_tree;


std::string extract_body(const Response& response) {
    std::string raw = response.to_string();
    std::string delimiter = "\r\n\r\n";
    size_t pos = raw.find(delimiter);
    return (pos != std::string::npos) ? raw.substr(pos + delimiter.size()) : "";
}

class CrudApiHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / "crud_api_test";
        fs::create_directories(temp_dir_);

        // call the factory, then cast its RequestHandler* to CrudApiHandler*
        RequestHandler* raw = CrudApiHandler::Init(
            "/api",
            {{ "root", temp_dir_.string() }}
        );

        handler_.reset(static_cast<CrudApiHandler*>(raw));
    }

    void TearDown() override {
        fs::remove_all(temp_dir_);
    }

    void create_test_file(const std::string& name,
                          const std::string& content) {
        std::ofstream((temp_dir_ / name).c_str()) << content;
    }

    fs::path temp_dir_;
    std::unique_ptr<CrudApiHandler> handler_;
};

TEST_F(CrudApiHandlerTest, PostValidJsonCreatesEntityFile) {
    std::string entity = "user";
    std::string body = R"({"username": "testuser"})";
    std::string request_text = 
    "POST /api/" + entity + " HTTP/1.1\r\n" +
    "Content-Length: " + std::to_string(body.size()) + "\r\n" +
    "Content-Type: application/json\r\n\r\n" +
    body;

    Request request(request_text);

    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 200);

    std::string response_body = extract_body(response);
    std::stringstream ss(response_body);
    pt::ptree json_response;

    try {
        pt::read_json(ss, json_response);

        boost::optional<int> id_opt = json_response.get_optional<int>("id");
        ASSERT_TRUE(id_opt);
        int id = id_opt.get();

        fs::path expected_path = temp_dir_ / entity / std::to_string(id);
        ASSERT_TRUE(fs::exists(expected_path));

        std::ifstream ifs(expected_path);
        std::string file_content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_EQ(file_content, body);


    } catch (const pt::json_parser::json_parser_error& e) {
        FAIL() << "JSON parsing error in response body: " << e.what();
    } catch (const std::exception& e) {
        FAIL() << "Error processing response body: " << e.what();
    }
}

TEST_F(CrudApiHandlerTest, PostInvalidJsonReturns400) {
    std::string entity = "user";
    std::string body = R"({invalid json)";  // malformed JSON
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
        body;
    Request request(request_text);

    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Invalid JSON") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PostMissingEntityReturns400) {
    std::string body = R"({"some": "data"})";
    std::string request_text = 
        std::string("POST /api/ HTTP/1.1\r\n") +
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Missing entity type") != std::string::npos);
}


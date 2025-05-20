#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "crud_api_handler.h"
#include "mock_filesystem.h"
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
        // create a mock filesystem
        mock_fs_ = std::make_shared<MockFileSystem>();
        
        // set up a temporary directory path
        temp_dir_ = "/tmp/crud_api_test";
        
        // initialize the handler with the mock filesystem
        handler_ = std::make_unique<CrudApiHandler>("/api", temp_dir_, mock_fs_);
        
        // create the base directory in mock filesystem
        mock_fs_->add_directory(temp_dir_);
    }

    void TearDown() override {
        // clear mock filesystem
        mock_fs_->clear();
        
        // Reset pointers
        handler_.reset();
        mock_fs_.reset();
    }

    void create_test_file(const std::string& name, const std::string& content) {
        fs::path full_path = fs::path(temp_dir_) / name;
        mock_fs_->add_file(full_path, content);
    }

    fs::path temp_dir_;
    std::shared_ptr<MockFileSystem> mock_fs_;
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

        fs::path expected_path = fs::path(temp_dir_) / entity / std::to_string(id);
        ASSERT_TRUE(mock_fs_->exists(expected_path));

        std::string file_content = mock_fs_->read_file(expected_path);
        EXPECT_EQ(file_content, body);

    } catch (const pt::json_parser::json_parser_error& e) {
        FAIL() << "JSON parsing error in response body: " << e.what();
    } catch (const std::exception& e) {
        FAIL() << "Error processing response body: " << e.what();
    }
}

TEST_F(CrudApiHandlerTest, PostMultipleRequestsCreatesUniqueFiles) {
    std::string entity = "user";
    std::vector<std::string> bodies = {
        R"({"username": "user1"})",
        R"({"username": "user2"})",
        R"({"username": "user3"})"
    };

    std::vector<int> ids;

    for (const auto& body : bodies) {
        std::string request_text =
            "POST /api/" + entity + " HTTP/1.1\r\n" +
            "Content-Length: " + std::to_string(body.size()) + "\r\n" +
            "Content-Type: application/json\r\n\r\n" +
            body;

        Request request(request_text);
        Response response = handler_->handle_request(request);

        EXPECT_EQ(response.get_status_code(), 200);

        std::string response_body = extract_body(response);
        std::stringstream ss(response_body);
        pt::ptree json_response;

        try {
            pt::read_json(ss, json_response);
            boost::optional<int> id_opt = json_response.get_optional<int>("id");
            ASSERT_TRUE(id_opt);
            int id = id_opt.get();
            ids.push_back(id);

            // Confirm file creation
            fs::path expected_path = fs::path(temp_dir_) / entity / std::to_string(id);
            ASSERT_TRUE(mock_fs_->exists(expected_path));

            std::string file_content = mock_fs_->read_file(expected_path);
            EXPECT_EQ(file_content, body);

        } catch (const std::exception& e) {
            FAIL() << "Error parsing response or checking file: " << e.what();
        }
    }

    // Ensure all IDs are unique
    std::sort(ids.begin(), ids.end());
    auto unique_end = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(unique_end, ids.end()) << "Duplicate IDs were assigned in POST responses";
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

TEST_F(CrudApiHandlerTest, GetValidEntityReturns200) {
    std::string entity = "user";
    std::string id = "1";
    std::string content = R"({"username": "testuser"})";

    // Create a test file for the entity
    create_test_file(entity + "/" + id, content);

    std::string request_text = 
        "GET /api/" + entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 200);

    std::string response_body = extract_body(response);
    EXPECT_EQ(response_body, content);
}

TEST_F(CrudApiHandlerTest, GetInvalidEntityReturns400) {
    std::string entity = "user";
    std::string invalid_entity = "shoe";
    std::string id = "1";
    std::string content = R"({"username": "testuser"})";

    // Create a test file for the entity
    create_test_file(entity + "/" + id, content);

    std::string request_text = 
        "GET /api/" + invalid_entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Entity type does not exist") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, GetNonExistentIDReturns400) {
    std::string entity = "user";
    std::string id = "1";
    std::string invalid_id = "2";
    std::string content = R"({"username": "testuser"})";

    // Create a test file for the entity
    create_test_file(entity + "/" + id, content);

    std::string request_text = 
        "GET /api/" + entity + "/" + invalid_id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("ID does not exist") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, GetListAllEntitiesReturns200) {
    std::string entity = "user";
    std::string content = R"([1, 2, 3])";  // Simulated entity IDs in an array

    // Create test files for the entity
    create_test_file(entity + "/1", "{\"username\": \"user1\"}");
    create_test_file(entity + "/2", "{\"username\": \"user2\"}");
    create_test_file(entity + "/3", "{\"username\": \"user3\"}");

    std::string request_text = 
        "GET /api/" + entity + " HTTP/1.1\r\n\r\n"; // Get all entities

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 200);

    std::string response_body = extract_body(response);
    std::cout << response_body;
    EXPECT_EQ(response_body, content);
}

TEST_F(CrudApiHandlerTest, GetListInvalidEntityReturns400) {
    std::string entity = "user";
    std::string invalid_entity = "shoe";
    std::string id = "1";
    std::string content = R"({"username": "testuser"})";

    // Create a test file for the entity
    create_test_file(entity + "/" + id, content);

    std::string request_text = 
        "GET /api/" + invalid_entity + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify status
    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Entity type does not exist") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PutCreatesEntityWithSpecificID) {
    std::string entity = "user";
    std::string id = "42";
    std::string body = R"({"username": "newuser"})";
    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 200);

    fs::path expected_path = temp_dir_ / entity / id;
    ASSERT_TRUE(mock_fs_->exists(expected_path));

    std::string file_content = mock_fs_->read_file(expected_path);
    EXPECT_EQ(file_content, body);
}

TEST_F(CrudApiHandlerTest, PutUpdatesExistingEntity) {
    std::string entity = "user";
    std::string id = "1";
    std::string original_content = R"({"username": "olduser"})";
    std::string updated_content = R"({"username": "updateduser"})";

    create_test_file(entity + "/" + id, original_content);

    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(updated_content.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        updated_content;

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 200);

    fs::path expected_path = temp_dir_ / entity / id;
    ASSERT_TRUE(mock_fs_->exists(expected_path));

    std::string file_content = mock_fs_->read_file(expected_path);
    EXPECT_EQ(file_content, updated_content);
}

TEST_F(CrudApiHandlerTest, PutInvalidJsonReturns400) {
    std::string entity = "user";
    std::string id = "1";
    std::string body = R"({invalid json)";
    
    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Invalid JSON in request body") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PutMissingIDReturns400) {
    std::string body = R"({"some": "data"})";
    std::string entity = "user";
    std::string request_text = 
        "PUT /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("No ID provided") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PutMissingBodyReturns400) {
    std::string body = R"({"some": "data"})";
    std::string entity = "user";
    std::string id = "1";

    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Missing request body") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, DeleteEntityByID) {
    std::string entity = "user";
    std::string id = "123";
    std::string content = R"({"username": "deleteuser"})";

    // Create the entity file to delete
    create_test_file(entity + "/" + id, content);

    std::string request_text = 
        "DELETE /api/" + entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    // Verify deletion succeeded
    EXPECT_EQ(response.get_status_code(), 200);
    EXPECT_FALSE(mock_fs_->exists(temp_dir_ / entity / id));
}

TEST_F(CrudApiHandlerTest, DeleteNonExistentIDReturns404) {
    std::string entity = "user";
    std::string non_existent_id = "999";

    std::string request_text = 
        "DELETE /api/" + entity + "/" + non_existent_id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 404);
    EXPECT_TRUE(extract_body(response).find("File does not exist") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, DeleteFromInvalidEntityReturns404) {
    std::string invalid_entity = "invalid";
    std::string id = "1";

    std::string request_text = 
        "DELETE /api/" + invalid_entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 404);
    EXPECT_TRUE(extract_body(response).find("File does not exist") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, DeleteMissingIDReturns400) {
    std::string entity = "user"; 
    std::string request_text = "DELETE /api/" + entity + "/ HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Missing entity type or ID") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, DeleteTargetIsNotAFileReturns400) {
    std::string entity = "user";
    std::string id = "directory_target";

    fs::path entity_dir = temp_dir_ / entity;
    mock_fs_->create_directories(entity_dir / id);

    std::string request_text = "DELETE /api/" + entity + "/" + id + " HTTP/1.1\r\n\r\n";
    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Target is not a file") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, InvalidUrlFormatReturns400) {
    std::string request_text = "GET /api////user HTTP/1.1\r\n\r\n";
    Request request(request_text);
    Response response = handler_->handle_request(request);

    EXPECT_EQ(response.get_status_code(), 400);
    EXPECT_TRUE(extract_body(response).find("Missing entity type in URL") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, UnsupportedHttpMethodReturns500) {
    std::string request_text = "PATCH /api/user/1 HTTP/1.1\r\n\r\n";
    Request request(request_text);
    Response response = handler_->handle_request(request);


    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Handler Not Implemented") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, WeaklyCanonicalReturnsPath) {
    std::string path = mock_fs_->weakly_canonical(temp_dir_);
    path = mock_fs_->canonical(temp_dir_);
    path = mock_fs_->read_symlink(temp_dir_);
    EXPECT_EQ(path, temp_dir_);
}

TEST_F(CrudApiHandlerTest, PostDirectoryCreationFailureReturns500) {
    std::string entity = "user";
    std::string body = R"({"username": "testuser"})";
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    
    //filesystem error when creating directories
    auto failing_fs = std::make_shared<MockFileSystem>();
    auto handler_with_failing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, failing_fs);
    
    //force a creation failure
    fs::path entity_dir_path = fs::path(temp_dir_) / entity;
    failing_fs->add_file(entity_dir_path, "This is a file, not a directory");
    
    Response response = handler_with_failing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Internal Server Error") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PostFileWriteFailureReturns500) {
    std::string entity = "user";
    std::string body = R"({"username": "testuser"})";
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    
    //failing write_file method
    class FailingWriteFileSystem : public MockFileSystem {
    public:
        bool write_file(const fs::path& path, const std::string& content) const override {
            return false;
        }
    };
    
    auto failing_fs = std::make_shared<FailingWriteFileSystem>();
    auto handler_with_failing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, failing_fs);
    
    //make sure directory exists
    failing_fs->add_directory(fs::path(temp_dir_) / entity);
    
    Response response = handler_with_failing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Could not open file for writing") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, GetFileReadFailureReturns500) {
    std::string entity = "user";
    std::string id = "1";
    
    //filesystem with a failing read_file method
    class FailingReadFileSystem : public MockFileSystem {
    public:
        using MockFileSystem::MockFileSystem;
        
        std::string read_file(const fs::path& path) const override {
            throw std::runtime_error("Simulated file read error");
        }
    };
    
    auto failing_fs = std::make_shared<FailingReadFileSystem>();
    auto handler_with_failing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, failing_fs);
    
    failing_fs->add_directory(fs::path(temp_dir_) / entity);
    failing_fs->add_file(fs::path(temp_dir_) / entity / id, "content doesn't matter");
    
    std::string request_text = 
        "GET /api/" + entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_with_failing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Failed to read file") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, GetFileSystemErrorReturns500) {
    std::string entity = "user";
    
    //filesystem that throws filesystem errors
    class ThrowingFileSystem : public MockFileSystem {
    public:
        bool is_directory(const fs::path& path) const override {
            throw fs::filesystem_error("Simulated filesystem error", 
                                      path, 
                                      std::make_error_code(std::errc::io_error));
        }
    };
    
    auto throwing_fs = std::make_shared<ThrowingFileSystem>();
    auto handler_with_throwing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, throwing_fs);
    
    std::string request_text = 
        "GET /api/" + entity + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_with_throwing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Filesystem error finding directory") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PutFileSystemErrorReturns500) {
    std::string entity = "user";
    std::string id = "1";
    std::string body = R"({"username": "testuser"})";
    
    //filesystem that throws filesystem errors
    class ThrowingCreateDirFileSystem : public MockFileSystem {
    public:
        bool create_directories(const fs::path& path) const override {
            throw fs::filesystem_error("Simulated filesystem error", 
                                      path, 
                                      std::make_error_code(std::errc::io_error));
        }
    };
    
    auto throwing_fs = std::make_shared<ThrowingCreateDirFileSystem>();
    auto handler_with_throwing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, throwing_fs);
    
    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_with_throwing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Could not create directory") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, PutWriteExceptionReturns500) {
    std::string entity = "user";
    std::string id = "1";
    std::string body = R"({"username": "testuser"})";
    
    //filesystem that throws on write
    class ThrowingWriteFileSystem : public MockFileSystem {
    public:
        bool write_file(const fs::path& path, const std::string& content) const override {
            throw std::runtime_error("Simulated write error");
        }
    };
    
    auto throwing_fs = std::make_shared<ThrowingWriteFileSystem>();
    auto handler_with_throwing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, throwing_fs);
    
    //add directory to avoid the directory creation error
    throwing_fs->add_directory(fs::path(temp_dir_) / entity);
    
    std::string request_text = 
        "PUT /api/" + entity + "/" + id + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_with_throwing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Error writing to file") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, InitMissingRootParameterThrowsException) {
    std::string location = "/api";
    std::unordered_map<std::string, std::string> params;
    //params is missing the 'root' key
    
    EXPECT_THROW({
        CrudApiHandler::Init(location, params);
    }, std::runtime_error);
}

TEST_F(CrudApiHandlerTest, DeleteWithFileSystemErrorReturns500) {
    std::string entity = "user";
    std::string id = "1";
    
    //filesystem that throws on remove
    class ThrowingRemoveFileSystem : public MockFileSystem {
    public:
        using MockFileSystem::MockFileSystem;
        
        bool remove(const fs::path& path) const override {
            throw fs::filesystem_error("Simulated remove error", 
                                      path, 
                                      std::make_error_code(std::errc::io_error));
        }
    };
    
    auto throwing_fs = std::make_shared<ThrowingRemoveFileSystem>();
    auto handler_with_throwing_fs = std::make_unique<CrudApiHandler>("/api", temp_dir_, throwing_fs);
    
    throwing_fs->add_directory(fs::path(temp_dir_) / entity);
    throwing_fs->add_file(fs::path(temp_dir_) / entity / id, "content");
    
    std::string request_text = 
        "DELETE /api/" + entity + "/" + id + " HTTP/1.1\r\n\r\n";

    Request request(request_text);
    Response response = handler_with_throwing_fs->handle_request(request);
    
    EXPECT_EQ(response.get_status_code(), 500);
    EXPECT_TRUE(extract_body(response).find("Filesystem error deleting file") != std::string::npos);
}

TEST_F(CrudApiHandlerTest, GenerateUniqueIdReturnsCorrectValue) {
    std::string entity = "user";
    
    //test files with numeric IDs
    mock_fs_->add_directory(fs::path(temp_dir_) / entity);
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "1", "content1");
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "5", "content5");
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "3", "content3");
    //non-numeric file that should be ignored
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "not_a_number", "content");
    
    std::string body = R"({"test": "data"})";
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);
    
    std::string response_body = extract_body(response);
    std::stringstream ss(response_body);
    pt::ptree json_response;
    pt::read_json(ss, json_response);
    int generated_id = json_response.get<int>("id");
    
    EXPECT_EQ(generated_id, 6);
}

TEST_F(CrudApiHandlerTest, GenerateUniqueIdWithNoExistingFiles) {
    std::string entity = "newentity";
    
    mock_fs_->add_directory(fs::path(temp_dir_) / entity);
    
    std::string body = R"({"test": "data"})";
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);
    
    std::string response_body = extract_body(response);
    std::stringstream ss(response_body);
    pt::ptree json_response;
    pt::read_json(ss, json_response);
    int generated_id = json_response.get<int>("id");
    
    EXPECT_EQ(generated_id, 1);
}

TEST_F(CrudApiHandlerTest, GenerateUniqueIdHandlesOutOfRangeIntegers) {
    std::string entity = "user";
    
    mock_fs_->add_directory(fs::path(temp_dir_) / entity);
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "1", "content1");
    mock_fs_->add_file(fs::path(temp_dir_) / entity / "99999999999999999999", "too_large");
    
    std::string body = R"({"test": "data"})";
    std::string request_text = 
        "POST /api/" + entity + " HTTP/1.1\r\n" +
        "Content-Length: " + std::to_string(body.size()) + "\r\n" +
        "Content-Type: application/json\r\n\r\n" +
        body;

    Request request(request_text);
    Response response = handler_->handle_request(request);
    
    std::string response_body = extract_body(response);
    std::stringstream ss(response_body);
    pt::ptree json_response;
    pt::read_json(ss, json_response);
    int generated_id = json_response.get<int>("id");
    
    EXPECT_EQ(generated_id, 2);
}

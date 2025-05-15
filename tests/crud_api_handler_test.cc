#include <gtest/gtest.h>
#include "crud_api_handler.h"
#include "request.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

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
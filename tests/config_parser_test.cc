#include <gtest/gtest.h>
#include <sstream>
#include <fstream>

#include "config_parser.h"

// ----------------  Base Fixture -----------------------
class ConfigTestBase : public testing::Test {
  protected:
    void TearDown() override {
      std::remove(test_config_path.c_str());
    }

    // Helper to write to test_config
    void WriteConfig(const std::string config_text) {
      std::ofstream f(test_config_path);
      f << config_text;
      f.close();
    }

    NginxConfigParser parser;
    NginxConfig out_config;
    std::string test_config_path = "/tmp/test_config";
};


// ----------------  NginxConfig unit tests  ------------

// Fixture for NginxConfig unit tests
class NginxConfigTest : public ConfigTestBase {
  protected:
    unsigned short port = 0;
};

// NginxConfig ExtractPort tests
TEST_F(NginxConfigTest, ExtractPortGoodPortNumber) {
  WriteConfig(R"(
    port 1234;
  )");
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  ASSERT_TRUE(out_config.ExtractPort(port));
  EXPECT_EQ(port, 1234);
}

TEST_F(NginxConfigTest, ExtractPortBadPortNumber) {
  WriteConfig(R"(
    port hello;
  )");
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_FALSE(out_config.ExtractPort(port));
}

TEST_F(NginxConfigTest, ExtractPortNoPortNumber) {
  WriteConfig(R"(
    port;
  )");
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_FALSE(out_config.ExtractPort(port));
}

TEST_F(NginxConfigTest, ExtractPortBadPortDirective) {
  WriteConfig(R"(
    part 1234;
  )");
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_FALSE(out_config.ExtractPort(port));
}

// NginxConfig ToString tests
TEST_F(NginxConfigTest, ToString) {
  std::string config_text = "port 80;\nserver {\n  listen 80;\n}\n";
  WriteConfig(config_text);
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_EQ(out_config.ToString(), config_text);
}

TEST_F(NginxConfigTest, ToStringWithDepth) {
  std::string config_text = "  port 80;\n  server {\n    listen 80;\n  }\n";
  WriteConfig(config_text);
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_EQ(out_config.ToString(1), config_text);
}

// NginxConfig ExtractRoutes tests
TEST_F(NginxConfigTest, ExtractRoutesGood) {
  std::string config_text = "port 80;\nroute / {\nhandler echo;\n}\nroute /static {\nhandler static;\nroot /;\n}";
  WriteConfig(config_text);
  std::vector<NginxConfig::RouteConfig> routes;
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  ASSERT_TRUE(out_config.ExtractRoutes(routes));
  EXPECT_EQ(routes.size(), 2);
  EXPECT_EQ(routes[0].path, "/");
  EXPECT_EQ(routes[0].handler_type, "echo");
  EXPECT_EQ(routes[1].path, "/static");
  EXPECT_EQ(routes[1].handler_type, "static");
}

TEST_F(NginxConfigTest, ExtractRoutesBadHandler) {
  std::string config_text = "port 80;\nroute / {\nhandler echo;\n}\nroute /static {\nbadhandler static;\nroot /;\n}";
  WriteConfig(config_text);
  std::vector<NginxConfig::RouteConfig> routes;
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  ASSERT_TRUE(out_config.ExtractRoutes(routes));
  EXPECT_EQ(routes.size(), 1);
  EXPECT_EQ(routes[0].path, "/");
  EXPECT_EQ(routes[0].handler_type, "echo");
}

TEST_F(NginxConfigTest, ExtractRoutesEmpty) {
  std::string config_text = "route / {\nbad;\n}";
  WriteConfig(config_text);
  std::vector<NginxConfig::RouteConfig> routes;
  ASSERT_TRUE(parser.Parse(test_config_path.c_str(), &out_config));
  EXPECT_FALSE(out_config.ExtractRoutes(routes));
}


// ----------------  NginxConfigParser unit tests  ------------

// Fixture for NginxConfigParser unit tests
class NginxConfigParserTest : public ConfigTestBase {};

// Bad Config File unit tests
TEST_F(NginxConfigParserTest, BadConfigFile) {
  bool success = parser.Parse("does_not_exist", &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, EmptyConfigFile) {
  WriteConfig("");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Double Quote unit tests
TEST_F(NginxConfigParserTest, DoubleQuote) {
  WriteConfig(R"(
    server_name "hello.com";
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, BadDoubleQuoteNoSpaceAfter) {
  WriteConfig(R"(
    server_name "hello".com;
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, BadDoubleQuoteNotClosed) {
  WriteConfig(R"(
    server_name "hello.com;
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, BadDoubleQuoteEndOfFile) {
  WriteConfig(R"(
    server_name "hello.com
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, DoubleQuoteEscape) {
  WriteConfig(R"(
    user "Lebron \"King\" James";
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, BadDoubleQuoteEscape) {
  WriteConfig(R"(
    user "Lebron "King" James";
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, BadDoubleQuoteEscapeEndOfFile) {
  WriteConfig(R"(user "Lebron \"King\" James\)");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Single Quote unit tests
TEST_F(NginxConfigParserTest, SingleQuote) {
  WriteConfig(R"(
    server_name 'hello.com';
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, BadSingleQuote) {
  WriteConfig(R"(
    server_name 'hello'.com;
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, SingleQuoteEscape) {
  WriteConfig(R"(
    user 'Lebron \'King\' James';
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, SingleQuoteEscapeV2) {
  WriteConfig(R"(
    server_name 'path\\to\\dir';
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, BadSingleQuoteEscape) {
  WriteConfig(R"(
    user 'Lebron 'King' James';
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, BadSingleQuoteEscapeEndOfFile) {
  WriteConfig(R"(user 'Lebron \'King\' James\)");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Bracket Tests
TEST_F(NginxConfigParserTest, NestedDirective) {
  WriteConfig(R"(
    server {
      listen   80;
      server_name foo.com;
      root /home/ubuntu/sites/foo/;
      location /one {
        goo "car";
      }
    }
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

TEST_F(NginxConfigParserTest, DoubleBracket) {
  WriteConfig(R"(
    server {{
      listen   80;
      root /home/ubuntu/sites/foo/;
    }
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, BadBlock) {
  WriteConfig(R"(
    {
      listen   80;
      root /home/ubuntu/sites/foo/;
    }
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, MissingBlockSemicolon) {
  WriteConfig(R"(
    server {
      listen   80;
      root /home/ubuntu/sites/foo/
    }
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Semicolon Tests
TEST_F(NginxConfigParserTest, MissingSemicolon) {
  WriteConfig(R"(
    port 80
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

TEST_F(NginxConfigParserTest, DoubleSemicolon) {
  WriteConfig(R"(
    port 80;;
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Comment Tests
TEST_F(NginxConfigParserTest, CommentedConfig) {
  WriteConfig(R"(
    # comment
    port 80;
    # comment #2
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

// Complex Config Tests
TEST_F(NginxConfigParserTest, ComplexConfig) {
  WriteConfig(R"(
    # Global settings
    app_name "Test \"Complex\" Config";
    debug_mode on;

    server {
        listen 8080;
        server_name "my.server.com";
        root "/home/user/sites/my_site";

        # Location block for API endpoints
        location /api {
            proxy_pass "http://backend:3000";
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            error_page 500 "Internal \"Server\" Error";
        }

        # Location block for static files
        location /static {
            alias "/home/user/sites/my_site/static";
            expires "30d";
        }

        error_page 404 "/custom_404.html";
    }

    # Stream configuration for TCP/UDP services
    stream {
        upstream backend {
            server 127.0.0.1:9000;
            server 127.0.0.2:9000;
        }

        server {
            listen 9000;
            proxy_pass backend;
        }
    }

    http {
        include mime.types;
        default_type application/octet-stream;

        log_format main '$remote_addr - $remote_user [$time_local] "$request" '
                        '$status $body_bytes_sent "$http_referer" '
                        '"$http_user_agent" "$http_x_forwarded_for"';

        access_log "/var/log/nginx/access.log" main;

        server {
            listen 80;
            server_name "www.example.com";
            location / {
                try_files $uri $uri/ =404;
            }
            location /downloads {
                # Note the use of escape sequence: the quote in this string is escaped.
                add_header Content-Disposition "attachment; filename=\"download.zip\"";
            }
        }
    }
  )");
  bool success = parser.Parse(test_config_path.c_str(), &out_config);
  EXPECT_TRUE(success);
}

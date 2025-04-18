#include <gtest/gtest.h>
#include <sstream>

#include "config_parser.h"

// ----------------  Config‑parser unit tests  -----------------
TEST(ConfigParserTest, ValidConfig) {
  NginxConfigParser p; NginxConfig cfg;
  std::stringstream ss("port 1234;");
  ASSERT_TRUE(p.Parse(&ss, &cfg));          //  ← use stream
  unsigned short port = 0;
  for (auto& s : cfg.statements_)
    if (s->tokens_[0] == "port") port = std::stoi(s->tokens_[1]);
  EXPECT_EQ(port, 1234);
}

TEST(ConfigParserTest, InvalidConfig) {
  NginxConfigParser p; NginxConfig cfg;
  std::stringstream ss("port bad_no_semicolon");
  EXPECT_FALSE(p.Parse(&ss, &cfg));         //  ← use stream
}
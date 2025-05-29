#include <gtest/gtest.h>
#include "markdown_converter.h"

#include <string>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
using namespace markdown;

// Normalize newlines to prevent CRLF/LF mismatch in test environments
std::string NormalizeNewlines(const std::string& input) {
    std::string output;
    for (char c : input) {
        if (c != '\r') output += c;
    }
    return output;
}

// Test fixture
class MarkdownConverterTest : public ::testing::Test {
protected:
    fs::path temp_file;

    void SetUp() override {
        temp_file = fs::temp_directory_path() / "markdown_test.md";
    }

    void TearDown() override {
        if (fs::exists(temp_file)) {
            fs::remove(temp_file);
        }
    }

    void WriteToFile(const std::string& content) {
        std::ofstream out(temp_file);
        out << content;
        out.close();
    }

    std::string LoadAndConvert() {
        std::ifstream in(temp_file);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return NormalizeNewlines(ConvertToHtml(buffer.str()));
    }

    void AssertContains(const std::string& html, const std::string& expected) {
        EXPECT_NE(html.find(expected), std::string::npos) << "Expected to find:\n" << expected << "\nIn:\n" << html;
    }
};

TEST_F(MarkdownConverterTest, ConvertsHeadingsAndText) {
    WriteToFile("# Heading\nSome **bold** text and *italic*.");
    std::string html = LoadAndConvert();

    AssertContains(html, "<h1>Heading</h1>");
    AssertContains(html, "<strong>bold</strong>");
    AssertContains(html, "<em>italic</em>");
}

TEST_F(MarkdownConverterTest, ConvertsListItems) {
    WriteToFile("- Item 1\n- Item 2");
    std::string html = LoadAndConvert();

    AssertContains(html, "<ul>");
    AssertContains(html, "<li>Item 1</li>");
    AssertContains(html, "<li>Item 2</li>");
}

TEST_F(MarkdownConverterTest, ConvertsInlineAndBlockCode) {
    WriteToFile("Here is `inline code`.\n\n```\nBlock code\n```");
    std::string html = LoadAndConvert();

    AssertContains(html, "<code>inline code</code>");
    AssertContains(html, "<pre><code>Block code\n</code></pre>");
}

TEST_F(MarkdownConverterTest, ConvertsLink) {
    WriteToFile("[OpenAI](https://openai.com)");
    std::string html = LoadAndConvert();

    AssertContains(html, "<a href=\"https://openai.com\">OpenAI</a>");
}

TEST_F(MarkdownConverterTest, WrapsInHtmlTemplate) {
    std::string raw = "<h1>Raw Body</h1>";
    std::string wrapped = WrapInHtmlTemplate(raw);

    AssertContains(wrapped, "<!DOCTYPE html>");
    AssertContains(wrapped, "<html");
    AssertContains(wrapped, "<body>");
    AssertContains(wrapped, raw);
    AssertContains(wrapped, "</html>");
}

// Note: cmark allows for nested lists, but they must be indented by four spaces or a tab. Cannot be just two.
TEST_F(MarkdownConverterTest, NestedLists) {
    WriteToFile("- Item 1\n    - Subitem 1.1\n    - Subitem 1.2\n- Item 2");
    std::string html = LoadAndConvert();

    AssertContains(html, "<ul>");
    AssertContains(html, "<li>Item 1");
    AssertContains(html, "<ul>");
    AssertContains(html, "<li>Subitem 1.1</li>");
    AssertContains(html, "<li>Subitem 1.2</li>");
    AssertContains(html, "</ul>");
    AssertContains(html, "<li>Item 2</li>");
}


TEST_F(MarkdownConverterTest, Blockquote) {
    WriteToFile("> This is a quote\n> With two lines.");
    std::string html = LoadAndConvert();

    AssertContains(html, "<blockquote>");
    AssertContains(html, "This is a quote");
    AssertContains(html, "With two lines.");
    AssertContains(html, "</blockquote>");
}

TEST_F(MarkdownConverterTest, HorizontalRule) {
    WriteToFile("Before\n\n---\n\nAfter");
    std::string html = LoadAndConvert();

    AssertContains(html, "<hr />");
}

TEST_F(MarkdownConverterTest, EscapedCharacters) {
    WriteToFile("\\*not italic\\* and \\# not a header");
    std::string html = LoadAndConvert();

    AssertContains(html, "*not italic*");
    AssertContains(html, "# not a header");
}

// I chose to not allow raw HTML passthrough, since this opens up security concerns for code injection attacks.
TEST_F(MarkdownConverterTest, RawHtmlPassthrough) {
    WriteToFile("<div>Raw HTML</div>");
    std::string html = LoadAndConvert();

    EXPECT_EQ(html.find("<div>Raw HTML</div>"), std::string::npos);
    EXPECT_NE(html.find("<!-- raw HTML omitted -->"), std::string::npos);

}

TEST_F(MarkdownConverterTest, MixedInlineElements) {
    WriteToFile("**bold _italic_ `code`**");
    std::string html = LoadAndConvert();

    AssertContains(html, "<strong>bold <em>italic</em> <code>code</code></strong>");
}

TEST_F(MarkdownConverterTest, HeaderWithFormatting) {
    WriteToFile("## Header with *italic* and `code`");
    std::string html = LoadAndConvert();

    AssertContains(html, "<h2>Header with <em>italic</em> and <code>code</code></h2>");
}

TEST_F(MarkdownConverterTest, UnclosedEmphasis) {
    WriteToFile("This *should not break");
    std::string html = LoadAndConvert();

    AssertContains(html, "*should not break");
}

TEST_F(MarkdownConverterTest, MultipleParagraphs) {
    WriteToFile("First paragraph.\n\nSecond paragraph.");
    std::string html = LoadAndConvert();

    AssertContains(html, "<p>First paragraph.</p>");
    AssertContains(html, "<p>Second paragraph.</p>");
}

TEST_F(MarkdownConverterTest, InlineHtmlInsideMarkdown) {
    WriteToFile("Some text with <span style=\"color:red\">inline HTML</span>.");
    std::string html = LoadAndConvert();

    EXPECT_EQ(html.find("<span style=\"color:red\">inline HTML</span>"), std::string::npos);
    EXPECT_NE(html.find("<!-- raw HTML omitted -->"), std::string::npos);

}

TEST_F(MarkdownConverterTest, TableNotRenderedByDefault) {
    WriteToFile("| Col1 | Col2 |\n|------|------|\n| Val1 | Val2 |");
    std::string html = LoadAndConvert();

    // Only expect plain text if tables are unsupported in your cmark build
    AssertContains(html, "| Col1 | Col2 |");
}


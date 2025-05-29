#ifndef MARKDOWN_CONVERTER_H
#define MARKDOWN_CONVERTER_H

#include <string>

namespace markdown {

    // Converts a Markdown string to HTML using the cmark library.
    std::string ConvertToHtml(const std::string& markdown_input);

    // (Optional) Wraps the HTML in a full HTML template with <html>, <head>, and <body> tags.
    std::string WrapInHtmlTemplate(const std::string& html_body);

}  // namespace markdown

#endif  // MARKDOWN_CONVERTER_H

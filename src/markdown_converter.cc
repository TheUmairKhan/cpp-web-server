#include "markdown_converter.h"
#include <cmark.h>

namespace markdown {

// Use CMARK_OPT_SAFE to strip raw HTML
std::string ConvertToHtml(const std::string& markdown_input) {
    char* html = cmark_markdown_to_html(markdown_input.c_str(),
                                        markdown_input.size(),
                                        CMARK_OPT_SAFE);

    // Null Check:
    std::string result;
    if (html != nullptr){
        result = std::string(html);
        free(html);
    }

    return result;
}

std::string WrapInHtmlTemplate(const std::string& html_body) {
    return "<!DOCTYPE html>\n"
           "<html lang=\"en\">\n"
           "<head>\n"
           "  <meta charset=\"UTF-8\">\n"
           "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
           "  <style>\n"
           "    body { font-family: Arial, sans-serif; padding: 2rem; line-height: 1.6; }\n"
           "    h1, h2, h3 { color: #333; }\n"
           "    code { background: #f4f4f4; padding: 0.2rem 0.4rem; border-radius: 4px; }\n"
           "    pre { background: #f4f4f4; padding: 1rem; border-radius: 4px; overflow-x: auto; }\n"
           "  </style>\n"
           "  <title>Markdown Render</title>\n"
           "</head>\n"
           "<body>\n"
           + html_body +
           "\n</body>\n"
           "</html>\n";
}

}  // namespace markdown

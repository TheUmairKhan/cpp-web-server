# Markdown Features Showcase

This page demonstrates various Markdown syntax elements and how they are converted to HTML by our server. Refresh your browser after editing to see the changes.

---

## 1. Headings

# H1 Heading  
## H2 Heading  
### H3 Heading  
#### H4 Heading  
##### H5 Heading  
###### H6 Heading  

---

## 2. Text Formatting

- **Bold text**  
- *Italic text*  
- ***Bold and Italic***  
- ~~Strikethrough~~  

---

## 3. Lists

### 3.1 Unordered List

- Item A
- Item B
  - Nested B.1
  - Nested B.2
- Item C

### 3.2 Ordered List

1. First step
2. Second step
   1. Sub-step a
   2. Sub-step b
3. Third step

---

## 4. Links & Images

- [Link to API Reference](api.md)  
- [External Link: Example.com](https://www.example.com)

- ![Sample Image](../static/images/sample.png)  
  *Alt text: “Sample Image”*

---

## 5. Blockquote

> “Markdown blockquotes become `<blockquote>` elements in HTML.”  
> — A Great Author

---

## 6. Tables

| Syntax Element | HTML Result                    |
| -------------- | ------------------------------ |
| `# H1 Heading` | `<h1>H1 Heading</h1>`         |
| `- List`       | `<ul><li>List</li></ul>`      |
| `**Bold**`     | `<strong>Bold</strong>`       |
| `` `code` ``   | `<code>code</code>`           |

---

## 7. Horizontal Rule

Three or more hyphens produce a `<hr>`:

---

## 8. Task List

- [x] Heading conversion  
- [x] Text formatting  
- [x] Lists (ordered & unordered)  
- [x] Links & images  
- [x] Blockquotes  
- [ ] Footnotes (if supported)  

---

## 9. Code Blocks

```cpp
#include <iostream>

// Example C++ code block to demonstrate fenced code conversion
int main() {
    std::cout << "Hello, Markdown!" << std::endl;
    return 0;
}

---

## 10. Footnote

Here is a footnote reference[^1] demonstrating a superscript link.

[^1]: This is the footnote definition displayed at the bottom.


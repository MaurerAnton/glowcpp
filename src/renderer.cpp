#include "renderer.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sstream>
#include <regex>

int terminalWidth() {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col > 0)
        return ws.ws_col;
    const char* cols = getenv("COLUMNS");
    if (cols) return atoi(cols);
    return 80;
}

/* ANSI style codes */
struct Style {
    const char* h1;       /* bright bold */
    const char* h2;
    const char* h3;
    const char* bold;
    const char* italic;
    const char* code;
    const char* codeBlock;
    const char* link;
    const char* blockquote;
    const char* listMarker;
    const char* hr;
    const char* strikethrough;
    const char* rst;
};

static Style darkStyle = {
    "\033[1;38;5;205m",  /* h1 - pink bold */
    "\033[1;38;5;111m",  /* h2 - blue bold */
    "\033[1;38;5;215m",  /* h3 - orange bold */
    "\033[1m",           /* bold */
    "\033[3m",           /* italic */
    "\033[7;38;5;252m",  /* inline code - reverse */
    "\033[38;5;246m",    /* code block - gray */
    "\033[4;38;5;75m",   /* link - underline cyan */
    "\033[38;5;246m",    /* blockquote - gray */
    "\033[38;5;215m",    /* list marker */
    "\033[38;5;240m",    /* hr */
    "\033[9m",           /* strikethrough */
    "\033[0m",
};

static Style lightStyle = {
    "\033[1;38;5;89m",   /* h1 */
    "\033[1;38;5;25m",   /* h2 */
    "\033[1;38;5;130m",  /* h3 */
    "\033[1m",
    "\033[3m",
    "\033[7;38;5;238m",
    "\033[38;5;243m",
    "\033[4;38;5;26m",
    "\033[38;5;243m",
    "\033[38;5;130m",
    "\033[38;5;248m",
    "\033[9m",
    "\033[0m",
};

static Style noStyle = {
    "", "", "", "", "", "", "", "", "", "", "", "", ""
};

/* Word-wrap text to given width */
static std::string wrapText(const std::string& text, int width, int indent, int firstIndent) {
    if (text.empty()) return "";
    std::string out;
    std::string line;
    int curWidth = 0;
    bool first = true;

    /* Process word by word */
    size_t pos = 0;
    std::string word;
    while (pos <= text.size()) {
        if (pos == text.size() || text[pos] == ' ' || text[pos] == '\n') {
            if (!word.empty()) {
                int wordLen = (int)word.size();
                int indentSize = first ? firstIndent : indent;
                if (curWidth == 0) {
                    /* Start new line */
                    for (int i = 0; i < indentSize; i++) line += ' ';
                    curWidth = indentSize;
                }
                if (curWidth + wordLen + (curWidth > indentSize ? 1 : 0) > width) {
                    /* Word would exceed width, start new line */
                    out += line + "\n";
                    line.clear();
                    curWidth = 0;
                    for (int i = 0; i < indent; i++) line += ' ';
                    curWidth = indent;
                    first = false;
                }
                if (curWidth > indent || (curWidth == indent && first && firstIndent > 0)) {
                    line += ' ';
                    curWidth++;
                }
                line += word;
                curWidth += wordLen;
                word.clear();
            }
            if (pos < text.size() && text[pos] == '\n') {
                out += line + "\n";
                line.clear();
                curWidth = 0;
                first = true;
            }
        } else {
            word += text[pos];
        }
        if (pos == text.size()) break;
        pos++;
    }
    if (!line.empty()) out += line;
    return out;
}

/* Render inline formatting: *italic*, **bold**, `code`, [text](url), ~~strike~~ */
static std::string renderInline(const std::string& text, const Style& s) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        /* Bold+Italic: *** */
        if (i + 2 < text.size() && text[i] == '*' && text[i+1] == '*' && text[i+2] == '*') {
            size_t end = text.find("***", i + 3);
            if (end != std::string::npos) {
                out += s.bold; out += s.italic;
                out += renderInline(text.substr(i + 3, end - i - 3), s);
                out += s.rst;
                i = end + 3;
                continue;
            }
        }
        /* Bold: ** */
        if (i + 1 < text.size() && text[i] == '*' && text[i+1] == '*') {
            size_t end = text.find("**", i + 2);
            if (end != std::string::npos) {
                out += s.bold;
                out += renderInline(text.substr(i + 2, end - i - 2), s);
                out += s.rst;
                i = end + 2;
                continue;
            }
        }
        /* Italic: * */
        if (text[i] == '*' && (i == 0 || text[i-1] != '\\')) {
            size_t end = text.find("*", i + 1);
            if (end != std::string::npos && end > i + 1) {
                out += s.italic;
                out += renderInline(text.substr(i + 1, end - i - 1), s);
                out += s.rst;
                i = end + 1;
                continue;
            }
        }
        /* Inline code: ` */
        if (text[i] == '`') {
            size_t end = text.find('`', i + 1);
            if (end != std::string::npos) {
                out += s.code;
                out += text.substr(i + 1, end - i - 1);
                out += s.rst;
                i = end + 1;
                continue;
            }
        }
        /* Strikethrough: ~~ */
        if (i + 1 < text.size() && text[i] == '~' && text[i+1] == '~') {
            size_t end = text.find("~~", i + 2);
            if (end != std::string::npos) {
                out += s.strikethrough;
                out += text.substr(i + 2, end - i - 2);
                out += s.rst;
                i = end + 2;
                continue;
            }
        }
        /* Link: [text](url) */
        if (text[i] == '[') {
            size_t closeBr = text.find("](", i);
            if (closeBr != std::string::npos) {
                size_t closeParen = text.find(")", closeBr + 2);
                if (closeParen != std::string::npos) {
                    std::string linkText = text.substr(i + 1, closeBr - i - 1);
                    std::string url = text.substr(closeBr + 2, closeParen - closeBr - 2);
                    out += std::string(s.link);
                    out += linkText;
                    out += s.rst;
                    out += " " + std::string(s.listMarker) + "(" + url + ")" + s.rst;
                    i = closeParen + 1;
                    continue;
                }
            }
        }
        /* Image: ![alt](url) */
        if (i + 1 < text.size() && text[i] == '!' && text[i+1] == '[') {
            size_t closeBr = text.find("](", i + 1);
            if (closeBr != std::string::npos) {
                size_t closeParen = text.find(")", closeBr + 2);
                if (closeParen != std::string::npos) {
                    std::string alt = text.substr(i + 2, closeBr - i - 2);
                    std::string url = text.substr(closeBr + 2, closeParen - closeBr - 2);
                    out += std::string(s.listMarker) + "[Image: " + alt + "](" + url + ")" + s.rst;
                    i = closeParen + 1;
                    continue;
                }
            }
        }
        out += text[i];
        i++;
    }
    return out;
}

std::string renderMarkdown(const std::string& markdown, int width, const std::string& styleName) {
    if (width <= 0) width = terminalWidth();

    const Style& s = (styleName == "light") ? lightStyle :
                     (styleName == "notty") ? noStyle : darkStyle;

    std::string out;
    std::istringstream ss(markdown);
    std::string line;
    bool inCodeBlock = false;
    std::string codeLang;

    while (std::getline(ss, line)) {
        /* Strip \r */
        if (!line.empty() && line.back() == '\r') line.pop_back();

        /* Code block handling */
        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeLang = line.substr(3);
                /* Trim language */
                size_t s2 = 0; while (s2 < codeLang.size() && codeLang[s2] == ' ') s2++;
                codeLang = codeLang.substr(s2);
                out += s.codeBlock;
                continue;
            } else {
                inCodeBlock = false;
                out += s.rst;
                continue;
            }
        }

        if (inCodeBlock) {
            out += line + "\n";
            continue;
        }

        /* Skip empty lines */
        /* Actually render them as blank lines */
        bool isEmpty = true;
        for (char c : line) if (c != ' ' && c != '\t') { isEmpty = false; break; }
        if (isEmpty) {
            out += "\n";
            continue;
        }

        /* Trim trailing spaces */
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();

        /* Horizontal rule: ---, ***, ___ (3+ chars) */
        if (line.size() >= 3) {
            char c = line[0];
            if ((c == '-' || c == '*' || c == '_') && line.find_first_not_of(std::string(1,c)+std::string(" ")) == std::string::npos) {
                out += s.hr + std::string(width, c) + s.rst + "\n\n";
                continue;
            }
        }

        /* Headers: # to ###### */
        int hLevel = 0;
        while (hLevel < (int)line.size() && line[hLevel] == '#') hLevel++;
        if (hLevel > 0 && hLevel <= 6 && hLevel < (int)line.size() && line[hLevel] == ' ') {
            std::string headerText = line.substr(hLevel + 1);
            /* Strip trailing # */
            while (!headerText.empty() && headerText.back() == '#') headerText.pop_back();
            while (!headerText.empty() && headerText.back() == ' ') headerText.pop_back();

            headerText = renderInline(headerText, s);
            if (hLevel == 1) {
                out += std::string("\n") + s.h1 + headerText + std::string(s.rst) + "\n";
            } else if (hLevel == 2) {
                out += std::string("\n") + s.h2 + headerText + std::string(s.rst) + "\n";
            } else {
                out += std::string(s.h3) + headerText + std::string(s.rst) + "\n";
            }
            continue;
        }

        /* Blockquote: > */
        if (!line.empty() && line[0] == '>') {
            std::string qtext = line.substr(1);
            if (!qtext.empty() && qtext[0] == ' ') qtext = qtext.substr(1);
            qtext = renderInline(qtext, s);
            std::string wrapped = wrapText(qtext, width - 4, 2, 2);
            std::string result;
            size_t pos = 0;
            while (pos < wrapped.size()) {
                size_t nl = wrapped.find('\n', pos);
                std::string l = (nl == std::string::npos) ? wrapped.substr(pos) : wrapped.substr(pos, nl - pos);
                result += std::string(s.blockquote) + "│ " + l + std::string(s.rst) + "\n";
                if (nl == std::string::npos) break;
                pos = nl + 1;
            }
            out += result;
            continue;
        }

        /* Unordered list: - * + */
        if ((line[0] == '-' || line[0] == '*' || line[0] == '+') && line.size() > 1 && line[1] == ' ') {
            std::string item = line.substr(2);
            item = renderInline(item, s);
            std::string wrapped = wrapText(item, width - 4, 4, 2);
            std::string marker = std::string(1, line[0]);
            /* Find first line of wrapped */
            size_t nl = wrapped.find('\n');
            if (nl == std::string::npos) {
                out += std::string("  ") + s.listMarker + marker + " " + s.rst + wrapped + "\n";
            } else {
                std::string first = wrapped.substr(0, nl);
                std::string rest = wrapped.substr(nl + 1);
                out += std::string("  ") + s.listMarker + marker + " " + s.rst + first + "\n" + rest + "\n";
            }
            continue;
        }

        /* Ordered list: 1. 2. etc */
        size_t dotPos = 0;
        while (dotPos < line.size() && line[dotPos] >= '0' && line[dotPos] <= '9') dotPos++;
        if (dotPos > 0 && dotPos < line.size() && line[dotPos] == '.' && dotPos + 1 < line.size() && line[dotPos + 1] == ' ') {
            std::string num = line.substr(0, dotPos);
            std::string item = line.substr(dotPos + 2);
            item = renderInline(item, s);
            std::string wrapped = wrapText(item, width - 4, 4, 2 + (int)num.size());
            size_t nl = wrapped.find('\n');
            if (nl == std::string::npos) {
                out += std::string("  ") + s.listMarker + std::string(num) + ". " + s.rst + wrapped + "\n";
            } else {
                out += std::string("  ") + s.listMarker + std::string(num) + ". " + s.rst + wrapped.substr(0, nl) + "\n" + wrapped.substr(nl + 1) + "\n";
            }
            continue;
        }

        /* Regular paragraph text */
        std::string rendered = renderInline(line, s);
        std::string wrapped = wrapText(rendered, width, 0, 2);
        out += wrapped + "\n";
    }

    if (inCodeBlock) out += s.rst;
    return out;
}

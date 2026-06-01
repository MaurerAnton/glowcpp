#include "renderer.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char* VERSION = "0.1.0";

/* Read entire file */
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/* Read stdin */
static std::string readStdin() {
    std::stringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

/* Check if path is a directory */
static bool isDir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/* Find README files in a directory */
static std::string findReadme(const std::string& dir) {
    const char* names[] = {"README.md", "README", "Readme.md", "readme.md", "readme"};
    for (auto* name : names) {
        std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode))
            return path;
    }
    return "";
}

/* List markdown files in directory (recursive) */
static std::vector<std::string> findMarkdownFiles(const std::string& dir) {
    std::vector<std::string> results;
    DIR* d = opendir(dir.c_str());
    if (!d) return results;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        std::string path = dir + "/" + entry->d_name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            auto sub = findMarkdownFiles(path);
            results.insert(results.end(), sub.begin(), sub.end());
        } else if (S_ISREG(st.st_mode)) {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && (strcasecmp(ext, ".md") == 0 || strcasecmp(ext, ".markdown") == 0 || strcasecmp(ext, ".mdown") == 0)) {
                results.push_back(path);
            }
        }
    }
    closedir(d);
    return results;
}

/* Fetch URL content (simple HTTP GET) */
static std::string fetchURL(const std::string& url) {
    /* Use system curl if available */
    std::string cmd = "curl -sL --max-time 10 '" + url + "' 2>/dev/null";
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return "";
    char buf[4096];
    std::string result;
    while (fgets(buf, sizeof(buf), f)) result += buf;
    pclose(f);
    return result;
}

static void printUsage() {
    fprintf(stderr,
        "glowcpp - Render markdown on the CLI (C++ port of glow)\n"
        "Usage: glowcpp [flags] [source]\n\n"
        "Source can be a file, directory, URL, or '-' for stdin.\n"
        "If no source is given, looks for README.md in current dir.\n\n"
        "Flags:\n"
        "  -s, --style STYLE   Style: dark (default), light, notty\n"
        "  -w, --width N       Set output width (default: terminal width)\n"
        "  -a, --all           Show all markdown files in directory\n"
        "  -p, --pager         Pipe output to $PAGER\n"
        "  -v, --version       Print version\n"
        "  -h, --help          Show this help\n");
}

int main(int argc, char* argv[]) {
    std::string source;
    std::string style = "dark";
    int width = 0;
    bool allFiles = false;
    bool usePager = false;
    bool flagVersion = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { printUsage(); return 0; }
        else if (a == "-v" || a == "--version") flagVersion = true;
        else if (a == "-a" || a == "--all") allFiles = true;
        else if (a == "-p" || a == "--pager") usePager = true;
        else if ((a == "-s" || a == "--style") && i + 1 < argc) style = argv[++i];
        else if ((a == "-w" || a == "--width") && i + 1 < argc) width = atoi(argv[++i]);
        else if (a[0] != '-') source = a;
        else { fprintf(stderr, "Unknown flag: %s\n", a.c_str()); return 1; }
    }

    if (flagVersion) { printf("glowcpp %s\n", VERSION); return 0; }

    if (width <= 0) width = terminalWidth();

    std::string markdown;

    /* Determine source */
    if (source.empty()) {
        /* Look for README in current dir */
        std::string readme = findReadme(".");
        if (!readme.empty()) source = readme;
        else source = ".";
    }

    /* Stdin */
    if (source == "-") {
        markdown = readStdin();
    }
    /* URL */
    else if (source.find("://") != std::string::npos || source.find("github.com") != std::string::npos) {
        markdown = fetchURL(source);
        if (markdown.empty()) {
            fprintf(stderr, "Failed to fetch: %s\n", source.c_str());
            return 1;
        }
    }
    /* Directory */
    else if (isDir(source)) {
        if (allFiles) {
            auto files = findMarkdownFiles(source);
            for (auto& f : files) {
                std::string content = readFile(f);
                if (content.empty()) continue;
                std::string rendered = renderMarkdown(content, width, style);
                printf("\n%s%s%s\n", "Style().h2, f.c_str(), Style().rst33[1;38;5;111m", f.c_str(), "Style().h2, f.c_str(), Style().rst33[0m");
                printf("%s", rendered.c_str());
            }
            return 0;
        } else {
            std::string readme = findReadme(source);
            if (!readme.empty()) {
                markdown = readFile(readme);
            } else {
                fprintf(stderr, "No README found in '%s'\n", source.c_str());
                return 1;
            }
        }
    }
    /* File */
    else {
        markdown = readFile(source);
        if (markdown.empty()) {
            fprintf(stderr, "Cannot read: %s\n", source.c_str());
            return 1;
        }
    }

    /* Render */
    std::string output = renderMarkdown(markdown, width, style);

    /* Pager */
    if (usePager) {
        const char* pager = getenv("PAGER");
        if (!pager) pager = "less -R";
        FILE* p = popen(pager, "w");
        if (p) {
            fputs(output.c_str(), p);
            pclose(p);
        } else {
            fputs(output.c_str(), stdout);
        }
    } else {
        fputs(output.c_str(), stdout);
    }

    return 0;
}

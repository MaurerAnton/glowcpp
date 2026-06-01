#ifndef GLOWCPP_RENDERER_HPP
#define GLOWCPP_RENDERER_HPP

#include <string>

/* Render markdown text to ANSI terminal output.
   width: terminal width in characters (0 = auto-detect).
   style: "dark", "light", "notty" (no color). */
std::string renderMarkdown(const std::string& markdown, int width, const std::string& style);

/* Detect terminal width */
int terminalWidth();

#endif

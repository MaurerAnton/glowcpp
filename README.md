# glowcpp — Terminal Markdown Renderer (C++ port of Glow)

A zero-dependency C++ port of [glow](https://github.com/charmbracelet/glow) — render markdown documents beautifully in the terminal.

## Why glowcpp?

The original [glow](https://github.com/charmbracelet/glow) requires the Go toolchain plus dozens of modules and a full charmbracelet stack. glowcpp compiles with a single `make` using only C++17 and standard Linux headers.

## Quick Start

```bash
make
./glowcpp README.md
```

## Features

- Rendered markdown with ANSI colors and styles
- Headers, lists, code blocks, tables, blockquotes
- Syntax-highlighted code fences
- Hyperlink detection
- Word wrapping to terminal width
- Pager mode for long documents
- Light and dark themes

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make

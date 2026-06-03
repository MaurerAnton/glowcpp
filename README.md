# glowcpp — Terminal Markdown Renderer (C++ port of Glow)

A zero-dependency C++ port of [glow](https://github.com/charmbracelet/glow) — render markdown files with ANSI colors and styles in the terminal.

## Why glowcpp?

The original [glow](https://github.com/charmbracelet/glow) requires Go plus dozens of modules from the charmbracelet ecosystem. glowcpp compiles with a single `make` using only C++17.

## Quick Start

```bash
make
./glowcpp README.md
./glowcpp -s light document.md
./glowcpp -w 80 file.md         # Force output width
./glowcpp https://example.com/doc.md  # Fetch from URL
```

## Features

- Renders markdown files, directories (README.md or all `.md` files), URLs, or stdin (`-`)
- Color themes: dark (default), light, notty (no color)
- Configurable output width (`-w`)
- Pager support (`-p` pipes to `$PAGER`)
- `-a` flag to show all markdown files in a directory
- Source files: multi-file architecture with separate `renderer.hpp`

## Build

```bash
make
```
Requires: GCC 10+ or Clang 12+, GNU Make

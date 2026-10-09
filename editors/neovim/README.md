# YLang support for Neovim

This folder installs the generated YLang Tree-sitter parser into Neovim and enables highlighting for `.yl` files using Neovim's built-in Tree-sitter API. It does not require the `nvim-treesitter` plugin.

## Requirements

- A recent Neovim build with built-in Tree-sitter support (`vim.treesitter.start`)
- GCC or Clang
- The generated parser in `tree-sitter-ylang/src/parser.c`

## Install on Linux

From the root of the cloned YLang repository:

```sh
git switch dev/lsp-foundation
git pull --ff-only
bash editors/neovim/install.sh
```

The Bash installer compiles `parser.c` into `ylang.so`, installs it under Neovim's data directory, and installs the highlighting query and two small Lua files into your Neovim config. It refuses to overwrite conflicting existing config files.

Then open a YLang file:

```sh
nvim tree-sitter-ylang/examples/hello.yl
```

The file should have the `ylang` filetype and highlight keywords, types, function names/calls, variables, operators, comments, strings, and f-string interpolation.

## Install on Windows

Install a recent Neovim build and GCC (MSYS2 UCRT64 is supported). Close open Neovim instances before updating an installed parser, then run this from PowerShell at the repository root:

```powershell
.\\editors\\neovim\\install.ps1
```

The PowerShell installer builds `ylang.dll`, installs it under Neovim's data directory, and installs the filetype/highlighting configuration. It refuses to overwrite different configuration files. Keep the UCRT64 `bin` directory on `PATH` so Neovim can load compiler runtime dependencies.

## Verify in Neovim

Run these commands inside Neovim:

```vim
:set filetype?
:InspectTree
:checkhealth vim.treesitter
```

For a `.yl` file, `:set filetype?` should report `filetype=ylang`. `:InspectTree` shows the syntax tree. If highlighting does not start, confirm that the parser and query were installed under the paths printed by the installer, then restart Neovim.

## Uninstall

Remove only the files installed for YLang:

```sh
rm -f "${XDG_DATA_HOME:-$HOME/.local/share}/nvim/site/parser/ylang.so"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/nvim/queries/ylang/highlights.scm"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/nvim/ftdetect/ylang.lua"
rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/nvim/after/plugin/ylang-treesitter.lua"
```

The parser/query are syntax-only tooling; the YLang compiler remains responsible for semantic and type checking.

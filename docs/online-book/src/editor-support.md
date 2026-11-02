# Editor Support

YLang source files use the `.yl` extension. Official editor integrations are maintained
in the project repository under `editors/`.

## Visual Studio Code

The VS Code extension provides syntax highlighting, language configuration,
compiler diagnostics, and editor commands. When the YLang repository is opened
as a workspace, VS Code reads `.vscode/extensions.json` and recommends the
official YLang extension if it is not installed.

See the [VS Code extension guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/vscode).

## Neovim

The repository includes native filetype detection, syntax highlighting, and
compiler-backed diagnostics for Neovim. Install the integration through your
plugin manager or add `editors/nvim` to your runtime path. Once installed,
opening a `.yl` file activates YLang support automatically.

See the [Neovim guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/nvim).

## Vim

A lightweight Vim runtime package provides `.yl` filetype detection, syntax
highlighting, indentation defaults, and comment handling. Install it through a
Vim plugin manager; Vim does not provide a built-in marketplace or a standard
API for recommending a missing language plugin.

See the [Vim guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/vim).

## Zed

The Zed extension provides Tree-sitter syntax highlighting and an initial
language-server integration for diagnostics, completion, hover, go-to-definition,
and document symbols. Some language-server features are still based on
lightweight source scanning and are limited to the current file.

The project enables Zed's extension suggestions in `.zed/settings.json`.
First-time discovery through Zed's extension gallery requires the YLang
extension to be published there; until then, install it as a development
extension from `editors/zed`.

See the [Zed guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/zed).

## Install Vim and Neovim integrations without a plugin manager

Both editors include native package-directory support. Copy `editors/vim` into `~/.vim/pack/ylang/start/ylang` for Vim, or `editors/nvim` into `~/.local/share/nvim/site/pack/ylang/start/ylang` for Neovim. The editor loads the integration automatically on startup; see each repository guide for exact clone and copy commands.

Vim and Neovim do not expose a common built-in marketplace or missing-plugin recommendation API. YLang therefore documents their native package mechanism instead of promising an automatic popup that the editors cannot provide.

## Verify the language

Editor coloring is not a compiler check. Use the compiler itself to verify a
program:

```sh
ylang check hello.yl
ylang build hello.yl -o hello
./hello
```

A file can be highlighted by an editor even when it contains invalid YLang.
The compiler remains the source of truth for accepted syntax and semantics.

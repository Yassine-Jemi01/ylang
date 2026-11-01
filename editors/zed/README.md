<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/ylang-logo-light.svg">
    <img src="assets/ylang-logo-dark.svg" alt="YLang" width="300">
  </picture>
</p>

# YLang support for Zed

YLang syntax highlighting and language intelligence for `.yl` files. The extension uses Tree-sitter for syntax and the YLang Language Server Protocol (LSP) server for editor features.

## Install

1. Clone or download the [YLang repository](https://github.com/Yassine-Jemi01/ylang).
2. Build and install the compiler and LSP server:

   ```sh
   make
   sh ./editors/zed/install-lsp.sh
   ```

   The installer places `ylang` and `ylang-lsp` in `~/.local/bin`, and the Python server in `~/.local/share/ylang`. Ensure `~/.local/bin` is on your `PATH`. Python 3 is required.

3. In Zed, open the Command Palette (`Ctrl+Shift+P`).
4. Run **zed: install dev extension** and select the repository's `editors/zed` directory.
5. Restart Zed and open a `.yl` file.

Set `YLANG_COMPILER` to the compiler executable path if your compiler is installed somewhere other than `~/.local/bin/ylang`.

## LSP features

- Compiler diagnostics from `ylang check` while editing and saving.
- Completion for YLang keywords, built-in types, and symbols in the current file.
- Hover information for local declarations and built-in symbols.
- Go to definition for declarations in the current file.
- Document symbols for functions, structs, enums, and variables.

## Current limitations

This is the initial LSP foundation. Completion, hover, and definition are currently based on lightweight source scanning and are limited to the current file; they are not yet backed by the compiler's semantic symbol table. Formatting, rename, references, cross-file navigation, code actions, and debugging are not implemented. Compiler diagnostics depend on the output format of `ylang check`; malformed or incomplete source may not always produce a diagnostic.

The extension can be developed from this repository. The Zed registry supports extensions stored in a subdirectory, so the official registry can reference `editors/zed` directly. Until the registry submission is accepted and merged, install it as a development extension.

## Grammar development

The Tree-sitter grammar source is at the repository root in `grammar.js`. The Zed extension pins a specific grammar commit in `extension.toml`.

```sh
npm install --global tree-sitter-cli
tree-sitter generate
tree-sitter test
```

Run the corpus tests under `test/corpus` before updating the pinned grammar revision in `extension.toml`.

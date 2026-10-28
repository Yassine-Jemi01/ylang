# YLang support for Zed

This directory is a development extension for the Zed editor. It registers
`.yl` files as YLang and provides Tree-sitter-based syntax highlighting,
bracket matching, comment toggling, and block indentation.

The Tree-sitter grammar source is at the repository root in `grammar.js`.
The extension pins the grammar to a specific commit so Zed can fetch the
grammar reproducibly.

## Install as a development extension

1. Build or install Zed and open it.
2. Open the Extensions view.
3. Choose **Install Dev Extension** (or run `zed: install dev extension`).
4. Select the `editors/zed` directory from this repository.
5. Open a `.yl` file and choose **YLang** from the language selector if it
   is not selected automatically.

Zed may need Rust and the WASI SDK to compile the Tree-sitter grammar. Check
the Zed extension-development log if grammar installation fails.

## Current scope

This is initial editor support, not a language server. It provides syntax
highlighting and editor structure only; it does not yet provide compiler
diagnostics, completion, go-to-definition, formatting, or debugging inside
Zed. Those require a YLang language server or dedicated integrations.

The extension is nested in the main YLang repository for development. Publishing
it in Zed's extension gallery requires a dedicated extension repository and
the usual Zed extension submission process.

## Grammar development

The grammar uses the Tree-sitter CLI. From the repository root:

```sh
npm install --global tree-sitter-cli
tree-sitter generate
tree-sitter test
```

The corpus under `test/corpus` is the starting regression suite. Run these
commands before updating the pinned grammar revision in `extension.toml`.

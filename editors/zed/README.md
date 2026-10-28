<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/ylang-logo-light.svg">
    <img src="assets/ylang-logo-dark.svg" alt="YLang" width="300">
  </picture>
</p>

# YLang support for Zed

Syntax highlighting and editor structure for YLang `.yl` files, powered by Tree-sitter.

## Quick setup

1. Download or clone the [YLang repository](https://github.com/Yassine-Jemi01/ylang).
2. In Zed, open the **Command Palette** (`Ctrl+Shift+P`).
3. Run **zed: install dev extension** (shown as **Install Dev Extension** in the Extensions view).
4. Select the `editors/zed` directory inside the cloned repository.
5. Open a `.yl` file. If needed, choose **YLang** from the language selector.

If you already have the repository, skip the download step and select its existing `editors/zed` directory.

## What works

- `.yl` file association
- Tree-sitter syntax highlighting
- Bracket matching
- Comment toggling
- Block indentation

## Current limitations

This is syntax/editor-structure support, **not a language server**. Compiler diagnostics, completion, go-to-definition, formatting, and debugging are not available in Zed yet.

The extension is currently installed as a development extension from this repository. Publishing it in Zed's extension gallery requires a dedicated extension repository and Zed's submission process.

## Grammar development

The Tree-sitter grammar source is at the repository root in `grammar.js`. The Zed extension pins a specific grammar commit in `extension.toml` so Zed can fetch it reproducibly.

From the repository root, install the Tree-sitter CLI and run:

```sh
npm install --global tree-sitter-cli
tree-sitter generate
tree-sitter test
```

Run the corpus tests under `test/corpus` before updating the pinned grammar revision in `extension.toml`.

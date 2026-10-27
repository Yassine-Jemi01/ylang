# Editor Support

YLang source files use the `.yl` extension. Editor integrations are maintained
in the project repository under `editors/`.

## Visual Studio Code

The VS Code extension provides syntax highlighting and language configuration,
and it can run the real `ylang check` command to report compiler diagnostics.
It also includes a terminal-based debugging command where the platform and
debugger support it. See the [VS Code extension guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/vscode).

## Neovim

The repository includes syntax highlighting and diagnostic-parser support for
Neovim. The diagnostic integration depends on the installed YLang compiler.
See the [Neovim guide](https://github.com/Yassine-Jemi01/ylang/tree/main/editors/nvim).

## Zed

Initial Zed support is available as a development extension in
`editors/zed`. It uses Tree-sitter for syntax highlighting and configures
`.yl` files, line comments, bracket matching, and indentation.

To try it, open Zed's Extensions view, choose **Install Dev Extension**, and
select the `editors/zed` directory from a local clone of the YLang repository.
The extension may need Rust and the WASI SDK to build its grammar.

This first version does **not** include compiler diagnostics, autocomplete,
formatting, go-to-definition, or debugging in Zed. Those capabilities should
not be assumed until a YLang language server or matching integration is
implemented and tested.

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

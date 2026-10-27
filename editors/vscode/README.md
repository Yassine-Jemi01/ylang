# YLang Language Support for VS Code

<p align="center">
  <img src="icons/ylang-light.svg" alt="YLang logo" width="96">
</p>

This extension registers the `.yl` file extension as YLang and provides syntax highlighting, compiler diagnostics, the official YLang language icon, and Code Runner integration.

## Included

- YLang 2.2 syntax highlighting for `struct` and `enum` declarations, enum variants, exhaustive `match` arms, named `from ... import ...` imports, typed variables and struct fields, built-in namespaces, functions, operators, arrays, scientific-notation numbers, strings, f-strings, and both `//` and `/* ... */` comments.
- Matching brackets, automatic closing pairs, comment toggling, and basic indentation.
- Official YLang mark for light and dark editor themes.
- A default Code Runner executor for `.yl` files: compile with YLang, then run the generated executable.
- Syntax and semantic diagnostics from the real `ylang check` compiler command, including diagnostic codes, full explanations, and actionable compiler hints.
- Checks on open, while editing, and on save. Checks use a temporary copy of the current editor buffer, so unsaved edits are checked too.

## Requirements

- VS Code 1.85 or newer.
- The YLang compiler built locally.
- [Code Runner](https://marketplace.visualstudio.com/items?itemName=formulahendry.code-runner) for the **Run Code** shortcut.
- [Error Lens](https://marketplace.visualstudio.com/items?itemName=usernamehw.errorlens) for displaying diagnostic messages inline next to the affected source code. The language extension creates the diagnostics; Error Lens displays them.

Build the compiler from the repository root:

```sh
make clean
make
make test
```

## Install or update the local extension on Linux

From the root of the YLang repository:

```sh
mkdir -p ~/.vscode/extensions
ln -sfn "$PWD/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.6.0
```

Fully restart VS Code, or run **Developer: Reload Window** from the Command Palette.

## Debugging (experimental)

On Linux with GDB or macOS with LLDB, use **YLang: Debug Current File** from the Command Palette. The extension saves the active `.yl` file and opens the native debugger in the integrated terminal using the compiler's `debug` command. This is an initial terminal-based workflow, not a complete Debug Adapter Protocol implementation. Interactive debugging is not implemented on Windows yet.

## Diagnostics

The Problems panel shows compiler diagnostics. Hover over a diagnostic to read the compiler's full explanation and the `= help:` suggestion. Use **YLang: Check Current File** to check manually and **YLang: Show Output** to inspect compiler startup or checker logs.

## Code Runner

Save a `.yl` file and invoke **Run Code** (default shortcut: `Ctrl+Alt+N`). The configured executor compiles the source and runs the output executable in the same directory.

## Icon note

The extension contributes the official YLang mark as the language icon for light and dark themes. Third-party file icon themes may override language-icon mappings. The optional Catppuccin installer remains separate.

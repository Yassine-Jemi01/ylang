# YLang Language Support for VS Code

This extension registers the `.yl` file extension as YLang and provides syntax highlighting, compiler diagnostics, a language icon, and Code Runner integration.

## Included

- YLang 2.2 highlighting for `struct` and `enum` declarations, enum variants, exhaustive `match` arms, named `from ... import ...` imports, typed variables and struct fields, built-in namespaces, functions, operators, arrays, scientific-notation numbers, strings, f-strings, and both `//` and `/* ... */` comments.
- Matching brackets, automatic closing pairs, comment toggling, and basic indentation.
- Official YLang mark for light and dark themes, using the current blue/orange identity.
- A default Code Runner executor for `.yl` files: compile with YLang, then run the generated executable.
- Syntax and semantic diagnostics from the real `ylang check` compiler command, including current array/type diagnostics, diagnostic codes, full explanations, and actionable compiler hints in diagnostic details.
- Checks on open, while editing, and on save. Checks use a temporary copy of the current editor buffer, so unsaved edits are checked too. For saved files in writable directories, the temporary source is created beside the file so relative imports continue to resolve.

## Requirements

- VS Code 1.85 or newer.
- The YLang compiler built locally.
- [Code Runner](https://marketplace.visualstudio.com/items?itemName=formulahendry.code-runner) for the **Run Code** shortcut.
- [Error Lens](https://marketplace.visualstudio.com/items?itemName=usernamehw.errorlens) for displaying diagnostic messages inline next to the affected source code. The language extension creates the diagnostics; Error Lens displays them.

Build the compiler:

```sh
cd ~/Documents/ylang-1.0
make clean
make
make test
```

## Install or update the local extension on Linux

From the root of the YLang repository, run:

```sh
git pull --ff-only
mkdir -p ~/.vscode/extensions
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.1.0
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.2.0
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.3.0
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.4.0
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.6.0
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.6.0
```

Fully restart VS Code, or run **Developer: Reload Window** from the Command Palette.

## Debugging (experimental)

On Linux with GDB or macOS with LLDB, use **YLang: Debug Current File** from the Command Palette. The extension saves the active `.yl` file and opens the native debugger in the integrated terminal using the compiler's `debug` command.

This is an initial terminal-based debug workflow, not a complete Debug Adapter Protocol implementation. Breakpoints, stepping, call stack and variable inspection are provided by GDB/LLDB; some names may be generated C names. Interactive debug integration is not implemented on Windows yet.

Build the compiler with debug command support, install GDB (Linux) or LLDB (macOS), and set `ylang.compilerPath` if VS Code cannot find your compiler.

## Diagnostic details

The Problems panel keeps inline messages compact. Hover over a diagnostic to read the compiler's full explanation and the `= help:` suggestion. Unknown or newly added diagnostic codes fall back to the compiler message, and unparsed compiler failures are logged to the YLang output channel rather than silently discarded.

## Configure the compiler path

VS Code started from the desktop may not inherit the PATH configured in fish. Open **Preferences: Open User Settings (JSON)** and add the compiler path under the top-level settings object. For the build in this repository:

```json
"ylang.compilerPath": "/home/yssn/Documents/ylang-1.0/build/ylang"
```

If you installed YLang to `~/.local/bin/ylang`, use that absolute path instead.

The checker runs `ylang check` and maps compiler errors/warnings to VS Code's Problems panel and source underlines. Run **YLang: Check Current File** from the Command Palette to request a check manually. Use **YLang: Show Output** to inspect compiler startup or checker logs.

## Test the diagnostic parser

No npm install is required. Run the diagnostic parser and YLang 2.2 syntax-grammar regression tests with Node.js:

```sh
cd editors/vscode
npm test
```

## Run programs with Code Runner

Save a `.yl` file and invoke **Run Code** (default shortcut: `Ctrl+Alt+N`). The configured executor compiles the source and runs the output executable in the same directory.

## File icon note

The extension provides the official YLang mark through VS Code's language-icon contribution. Third-party file icon themes may override this mapping. The separate local theme installer, `editors/vscode/install-catppuccin-icon.sh`, can add the YLang icon to a locally installed Catppuccin Mocha icon theme.

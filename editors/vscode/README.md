# YLang Language Support for VS Code

This extension registers the `.yl` file extension as YLang and provides syntax highlighting, compiler diagnostics, a language icon, and Code Runner integration.

## Included

- Highlighting for keywords, types, booleans, numbers, strings, characters, f-strings, function declarations/calls, operators, and `//` comments.
- Matching brackets, automatic closing pairs, comment toggling, and basic indentation.
- Light/dark YLang icons for icon themes that support language icons.
- A default Code Runner executor for `.yl` files: compile with YLang, then run the generated executable.
- Syntax and semantic diagnostics from the real `ylang check` compiler command, including diagnostic codes and compiler hints.
- Checks on open, while editing, and on save. Checks use a temporary copy of the current editor buffer, so unsaved edits are checked too.

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
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.3.0
```

Fully restart VS Code, or run **Developer: Reload Window** from the Command Palette.

## Configure the compiler path

VS Code started from the desktop may not inherit the PATH configured in fish. Open **Preferences: Open User Settings (JSON)** and add the compiler path under the top-level settings object. For the build in this repository:

```json
"ylang.compilerPath": "/home/yssn/Documents/ylang-1.0/build/ylang"
```

If you installed YLang to `~/.local/bin/ylang`, use that absolute path instead.

The checker runs `ylang check` and maps compiler errors/warnings to VS Code's Problems panel and source underlines. Run **YLang: Check Current File** from the Command Palette to request a check manually. Use **YLang: Show Output** to inspect compiler startup or checker logs.

## Run programs with Code Runner

Save a `.yl` file and invoke **Run Code** (default shortcut: `Ctrl+Alt+N`). The configured executor compiles the source and runs the output executable in the same directory.

## File icon note

The extension provides light and dark YLang icons through VS Code's language-icon contribution. Third-party file icon themes may override this mapping. The separate local theme installer, `editors/vscode/install-catppuccin-icon.sh`, can add the YLang icon to a locally installed Catppuccin Mocha icon theme.

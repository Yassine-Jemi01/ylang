# YLang Language Support for VS Code

This extension registers the `.yl` file extension as YLang and adds TextMate syntax highlighting for the language features documented in YLang 1.0.0.

## Included

- Highlighting for keywords, types, booleans, numbers, strings, characters, f-strings, function declarations/calls, operators, and `//` comments.
- Matching brackets, automatic closing pairs, comment toggling, and basic indentation.

This is syntax highlighting and basic editor configuration. It does not yet provide compiler diagnostics, autocomplete, go-to-definition, or debugging.

## Local installation on Linux

From the root of the YLang repository, run:

```sh
mkdir -p ~/.vscode/extensions
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.1.0
```

Restart VS Code, or run **Developer: Reload Window** from the Command Palette. Open any `.yl` file; the language mode should show **YLang**.

If the symlink already exists, remove that link before running the `ln -s` command again. Do not remove your repository directory.

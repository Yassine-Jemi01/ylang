# YLang Language Support for VS Code

This extension registers the `.yl` file extension as YLang and adds TextMate syntax highlighting for the language features documented in YLang 1.0.0.

## Included

- Highlighting for keywords, types, booleans, numbers, strings, characters, f-strings, function declarations/calls, operators, and `//` comments.
- Matching brackets, automatic closing pairs, comment toggling, and basic indentation.
- A YLang language icon for VS Code file-icon themes that support language icons.
- A default Code Runner executor for `.yl` files: compile with YLang, then run the generated executable.

The Code Runner integration requires the separate **Code Runner** extension and the `ylang` compiler command to be available on `PATH`. This extension does not yet provide compiler diagnostics, autocomplete, go-to-definition, or debugging.

## Requirements

1. Install the [Code Runner extension](https://marketplace.visualstudio.com/items?itemName=formulahendry.code-runner) if you want to use **Run Code**.
2. Build and install the YLang compiler to `~/.local/bin/ylang`:

   ```sh
   cd ~/Documents/ylang-1.0
   make clean
   make
   make test
   make install PREFIX="$HOME/.local"
   ```

3. Ensure `~/.local/bin` is on your `PATH`. In fish, run:

   ```fish
   fish_add_path "$HOME/.local/bin"
   ylang --version
   ```

   If VS Code was already open, fully restart it after changing your `PATH`.

## Local installation on Linux

From the root of the YLang repository, run:

```sh
mkdir -p ~/.vscode/extensions
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.1.0
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.2.0
```

Restart VS Code, or run **Developer: Reload Window** from the Command Palette. Open any `.yl` file; the language mode should show **YLang**.

To run a program, save the `.yl` file and invoke **Run Code** from Code Runner (default shortcut: `Ctrl+Alt+N`). The configured executor runs:

```sh
ylang build <source-file.yl> -o <output-program>
./<output-program>
```

The executable is created in the same directory as the source file.

## File icon note

The extension provides light and dark YLang icons using VS Code's language-icon contribution. They show up with file-icon themes that support language icons and do not already define their own `.yl` icon. If your current icon theme uses a generic icon for `.yl`, try **Preferences: File Icon Theme** and select **Seti**. Some third-party icon themes manage their own extension associations.

If the symlink already exists under another name, remove only that symlink before creating the new one. Do not remove your repository directory.

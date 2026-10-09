# YLang Language Support for VS Code

YLang's VS Code extension provides syntax highlighting, a custom language icon, compiler-backed diagnostics, basic language-server features, and a cross-platform build/run command.

## Features

- TextMate syntax highlighting for the YLang 1.0 syntax.
- Automatic brackets and comment toggling.
- YLang icon support, including the local Catppuccin Mocha icon theme installer.
- **YLang: Build and Run Current File** command, which builds with the configured compiler and launches the result in an integrated terminal (including interactive stdin).
- LSP diagnostics from the actual `ylang check` compiler command.
- Basic completion for keywords, types, and built-in `print`.
- Hover information for built-in types and keywords, plus simple same-file function/variable information.
- Go to definition for function and variable declarations in the current file.
- Debounced checks while editing, plus checks on open and save.

This is an early LSP foundation. It does not yet offer full scope-aware symbol analysis, cross-file navigation, rename refactoring, code actions, formatting, or debugging.

## Requirements

- VS Code 1.91 or newer.
- Node.js and npm to install the LSP dependencies.
- The YLang compiler executable.
- [Error Lens](https://marketplace.visualstudio.com/items?itemName=usernamehw.errorlens) for inline diagnostic messages.

## Get the LSP development branch

From the local YLang repository:

```sh
git fetch origin
git switch --track origin/dev/lsp-foundation
cd editors/vscode
npm install
npm run check
```

The development branch is separate from the stable `main` branch. The first install downloads the dependencies used by the LSP client/server.

## Install the local extension

After switching to `dev/lsp-foundation` and installing dependencies:

```sh
mkdir -p ~/.vscode/extensions
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.3.0
rm -f ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.5.0
ln -s "$HOME/Documents/ylang-1.0/editors/vscode" ~/.vscode/extensions/yassine-jemi01.ylang-language-support-0.5.0
```

Build the compiler if needed:

```sh
cd ~/Documents/ylang-1.0
make
make test
```

Open VS Code settings JSON and set the compiler path to the local binary:

```json
"ylang.compilerPath": "/home/yssn/Documents/ylang-1.0/build/ylang"
```

Keep this setting inside your existing top-level settings object. Restart VS Code completely or run **Developer: Reload Window**.

## Use it

Open a `.yl` file. Diagnostics should appear in the Problems panel and as underlines; Error Lens can show the short messages inline. The extension works on Linux and Windows when the matching YLang compiler and native C compiler are installed.

- Run **YLang: Check Current File** from the Command Palette to request an immediate check.
- Press **Ctrl+Space** for basic keyword and type completion.
- Hover over a type, keyword, or simple function declaration for information.
- Use **F12** on a same-file function or variable usage to try go-to-definition.
- Run **YLang: Build and Run Current File** from the Command Palette to compile and launch the program in a terminal. This command works on Windows (`ylang.exe`) and Linux and keeps the terminal interactive for programs that read stdin.

The compiler is authoritative: diagnostic locations and semantic errors are produced by the existing YLang compiler rather than a second implementation of language rules.


## Windows setup

1. Install a GCC toolchain such as MSYS2 UCRT64 and make sure `gcc.exe` is available on `PATH`.
2. Build YLang on Windows with the repository's Makefile from an MSYS2 UCRT64 terminal (see [the Windows guide](../../docs/windows.md)).
3. Set `ylang.compilerPath` to the absolute path of `ylang.exe`, for example `C:\\msys64\\home\\your-user\\ylang\\build\\ylang.exe`.
4. Restart VS Code, open a `.yl` file, and run **YLang: Build and Run Current File**.

The language server invokes the configured compiler directly rather than through a shell, so Windows paths containing spaces are supported.

# YLang editor integrations

YLang source files use the `.yl` extension. Official editor integrations live
in this directory.

| Editor | Integration | Discovery / installation |
| --- | --- | --- |
| Visual Studio Code | `editors/vscode` | Workspace extension recommendation in `.vscode/extensions.json` |
| Zed | `editors/zed` | Zed's built-in extension suggestions; the extension must be published in the Zed gallery for first-time discovery |
| Neovim | `editors/nvim` | Install with a plugin manager; native filetype detection, syntax, and compiler diagnostics |
| Vim | `editors/vim` | Install with a Vim plugin manager; native filetype detection, syntax, and indentation |

## Install without a plugin manager

Vim and Neovim both support native package directories. The official integrations can be copied into their respective `pack/*/start` directories and loaded automatically, without installing vim-plug, lazy.nvim, or another third-party plugin manager. See the Vim and Neovim guides for exact commands.

Neither editor exposes a shared marketplace or a standard prompt API that can recommend an integration before it is installed. Native packages are therefore the supported editor-owned discovery alternative; they do not create an automatic missing-plugin popup.

## What happens when you open a `.yl` file?

- **VS Code:** when you open the repository as a workspace, VS Code reads
  `.vscode/extensions.json` and recommends the official YLang extension if it
  is not installed.
- **Zed:** extension suggestions are enabled in the project settings. Zed can
  recommend YLang once the extension is available in its extension gallery.
  The current repository extension is still installed as a development
  extension, so gallery publication is a separate release task.
- **Neovim and Vim:** these editors do not have a universal built-in extension
  marketplace or a standard missing-extension recommendation API. Their
  integrations are installed through the user's chosen plugin manager. Once
  installed, opening `.yl` automatically selects the YLang filetype and loads
  the language support.

No editor integration silently installs software. The user remains in control
of every installation.

## Compiler

Editor syntax support does not replace the compiler. Install the YLang
toolchain and make sure `ylang` is available on `PATH` to use compiler-backed
diagnostics.

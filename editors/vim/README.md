# YLang support for Vim

This lightweight Vim runtime package provides native filetype detection,
syntax highlighting, indentation defaults, and comment handling for `.yl`
files.

## Install with vim-plug

Add this to your `~/.vimrc`:

```vim
call plug#begin('~/.vim/plugged')
Plug 'Yassine-Jemi01/ylang', { 'rtp': 'editors/vim' }
call plug#end()
```

Then run `:PlugInstall` and restart Vim.

## Install with a native Vim package directory

Clone the repository and add the Vim integration directory to your runtime
path in `~/.vimrc`:

```vim
set runtimepath^=~/src/ylang/editors/vim
```

Replace the path with the location of your YLang checkout.

## Install without a plugin manager

Vim also supports native packages. Clone YLang and copy the runtime files into Vim's built-in `pack/*/start` directory:

```sh
git clone --depth 1 https://github.com/Yassine-Jemi01/ylang.git /tmp/ylang
mkdir -p ~/.vim/pack/ylang/start/ylang
cp -R /tmp/ylang/editors/vim/. ~/.vim/pack/ylang/start/ylang/
```

Restart Vim; the package is loaded automatically. To update it, repeat the clone into a temporary directory and replace the contents of `~/.vim/pack/ylang/start/ylang`.

## Features

- Detects `.yl` files as the `ylang` filetype.
- Syntax highlighting for declarations, types, control flow, strings,
  comments, operators, functions, and common built-ins.
- Four-space indentation defaults.
- `//` line comments.

## Compiler diagnostics

Vim's built-in runtime package currently provides syntax and editing support.
For compiler-backed diagnostics, use the official Neovim integration or
configure an LSP client once the YLang language server is available for Vim.

## Verify

Open a `.yl` file and run:

```vim
:set filetype?
```

The result should include `filetype=ylang`.

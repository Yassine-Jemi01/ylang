#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PLUGIN_FILE="$SCRIPT_DIR/plugin/ylang.lua"
DIAGNOSTICS_FILE="$SCRIPT_DIR/lua/ylang/diagnostics.lua"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
DEST="$DATA_HOME/nvim/site/pack/ylang/start/ylang-nvim"

if [ ! -f "$PLUGIN_FILE" ] || [ ! -f "$DIAGNOSTICS_FILE" ]; then
    echo "Could not locate the YLang Neovim plugin files in: $SCRIPT_DIR" >&2
    exit 1
fi

mkdir -p "$(dirname -- "$DEST")"

if [ -e "$DEST" ] && [ ! -L "$DEST" ]; then
    echo "Refusing to replace an existing non-symlink install: $DEST" >&2
    echo "Back it up or remove it manually, then run this installer again." >&2
    exit 1
fi

ln -sfn "$SCRIPT_DIR" "$DEST"
printf 'Installed YLang Neovim support to %s\n' "$DEST"
printf 'Set g:ylang_compiler_path in init.lua to the absolute path of your built ylang executable.\n'
printf 'Restart Neovim, open a .yl file, then run :YLangCheck.\n'

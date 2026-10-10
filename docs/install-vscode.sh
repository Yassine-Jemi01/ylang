#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EXT_DIR="${HOME}/.vscode/extensions"
DEST="$EXT_DIR/ylang-language-support"
mkdir -p "$EXT_DIR"
rm -rf "$DEST"
mkdir -p "$DEST"
cp -R "$ROOT"/editors/vscode/. "$DEST"/
printf 'Installed YLang VS Code support to %s\n' "$DEST"
printf 'Restart VS Code or run Developer: Reload Window.\n'

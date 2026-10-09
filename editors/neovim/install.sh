#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
GRAMMAR_DIR="$REPO_ROOT/tree-sitter-ylang"
CONFIG_ROOT="${XDG_CONFIG_HOME:-$HOME/.config}/nvim"
DATA_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/nvim"
PARSER_OUT="$DATA_ROOT/site/parser/ylang.so"
QUERY_OUT="$CONFIG_ROOT/queries/ylang/highlights.scm"
FILETYPE_OUT="$CONFIG_ROOT/ftdetect/ylang.lua"
PLUGIN_OUT="$CONFIG_ROOT/after/plugin/ylang-treesitter.lua"

die() {
  printf 'YLang Neovim installer: %s\n' "$*" >&2
  exit 1
}

command -v nvim >/dev/null 2>&1 || die "Neovim was not found."
command -v cc >/dev/null 2>&1 || die "A C compiler (GCC or Clang) is required."
[[ -f "$GRAMMAR_DIR/src/parser.c" ]] || die "Generated parser not found at $GRAMMAR_DIR/src/parser.c. Run npm install && npm run generate in tree-sitter-ylang first."
[[ -f "$GRAMMAR_DIR/queries/highlights.scm" ]] || die "Highlight query not found."

nvim --headless -u NONE \
  -c 'lua assert(vim.treesitter and vim.treesitter.start, "This Neovim build does not expose vim.treesitter.start; install a recent Neovim build.")' \
  -c 'qa!' >/dev/null 2>&1 \
  || die "This Neovim build lacks the built-in Tree-sitter start API. Install a recent Neovim build."

check_managed_file() {
  local source="$1"
  local destination="$2"
  if [[ -e "$destination" ]] && ! cmp -s "$source" "$destination"; then
    die "Refusing to overwrite a different existing config file: $destination
Back it up or merge it manually, then rerun this installer."
  fi
}

check_managed_file "$SCRIPT_DIR/ftdetect/ylang.lua" "$FILETYPE_OUT"
check_managed_file "$SCRIPT_DIR/after/plugin/ylang-treesitter.lua" "$PLUGIN_OUT"
check_managed_file "$GRAMMAR_DIR/queries/highlights.scm" "$QUERY_OUT"

temporary_parser="$(mktemp "${TMPDIR:-/tmp}/ylang-parser.XXXXXX.so")"
trap 'rm -f "$temporary_parser"' EXIT

cc -std=c11 -O2 -fPIC -shared \
  -I "$GRAMMAR_DIR/src" \
  "$GRAMMAR_DIR/src/parser.c" \
  -o "$temporary_parser"

install -Dm755 "$temporary_parser" "$PARSER_OUT"
install -Dm644 "$GRAMMAR_DIR/queries/highlights.scm" "$QUERY_OUT"
install -Dm644 "$SCRIPT_DIR/ftdetect/ylang.lua" "$FILETYPE_OUT"
install -Dm644 "$SCRIPT_DIR/after/plugin/ylang-treesitter.lua" "$PLUGIN_OUT"

nvim --headless -u NONE \
  -c "lua vim.opt.runtimepath:append(vim.fn.stdpath('data') .. '/site')" \
  -c 'lua vim.treesitter.language.add("ylang"); assert(vim.treesitter.query.get("ylang", "highlights"), "YLang highlights query could not be loaded")' \
  -c 'qa!' >/dev/null 2>&1 \
  || die "The parser or highlights query could not be loaded by Neovim. Check Neovim's Tree-sitter ABI compatibility."

printf 'YLang Tree-sitter installed successfully.\n'
printf '  Parser: %s\n' "$PARSER_OUT"
printf '  Query:  %s\n' "$QUERY_OUT"
printf '  Filetype config: %s\n' "$FILETYPE_OUT"
printf '  Highlight config: %s\n' "$PLUGIN_OUT"
printf 'Open a .yl file in Neovim. Use :InspectTree to inspect its syntax tree.\n'

#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/Yassine-Jemi01/ylang.git"
INSTALL_DIR="${HOME}/.local/share/ylang"
BIN_DIR="${HOME}/.local/bin"
EXTENSION_ID="yassine-jemi01.ylang-language-support"
EXTENSION_VERSION="0.6.0"

say() { printf '\n[YLang] %s\n' "$*"; }
fail() { printf '\n[YLang] ERROR: %s\n' "$*" >&2; exit 1; }

for tool in git make; do
  command -v "$tool" >/dev/null 2>&1 || fail "Missing '$tool'. Install git and make, then rerun this installer."
done

if [[ -d "$INSTALL_DIR/.git" ]]; then
  say "Updating existing checkout at $INSTALL_DIR"
  git -C "$INSTALL_DIR" pull --ff-only
elif [[ -e "$INSTALL_DIR" ]]; then
  fail "$INSTALL_DIR exists but is not a Git checkout. Move it aside and rerun."
else
  mkdir -p "$(dirname "$INSTALL_DIR")"
  say "Downloading YLang source"
  git clone --depth 1 "$REPO_URL" "$INSTALL_DIR"
fi

say "Building the YLang compiler"
make -C "$INSTALL_DIR"
mkdir -p "$BIN_DIR"
install -m 0755 "$INSTALL_DIR/build/ylang" "$BIN_DIR/ylang"

if [[ ":$PATH:" != *":$BIN_DIR:"* ]]; then
  case "${SHELL##*/}" in
    fish)
      mkdir -p "$HOME/.config/fish/conf.d"
      printf 'fish_add_path -g %s\n' "$BIN_DIR" > "$HOME/.config/fish/conf.d/ylang-path.fish"
      ;;
    *)
      PROFILE="$HOME/.profile"
      if ! grep -Fq 'HOME/.local/bin' "$PROFILE" 2>/dev/null; then
        printf '\n# YLang compiler\nexport PATH="$HOME/.local/bin:$PATH"\n' >> "$PROFILE"
      fi
      ;;
  esac
fi

install_editor() {
  local cli="$1" ext_dir="$2"
  command -v "$cli" >/dev/null 2>&1 || return 0
  say "Installing Code Runner and Error Lens in $cli"
  "$cli" --install-extension formulahendry.code-runner --force || say "Could not install Code Runner automatically; install it from the Extensions view."
  "$cli" --install-extension usernamehw.errorlens --force || say "Could not install Error Lens automatically; install it from the Extensions view."
  mkdir -p "$ext_dir"
  rm -rf "$ext_dir/$EXTENSION_ID-$EXTENSION_VERSION"
  ln -s "$INSTALL_DIR/editors/vscode" "$ext_dir/$EXTENSION_ID-$EXTENSION_VERSION"
}

install_editor code "$HOME/.vscode/extensions"
install_editor codium "$HOME/.vscode-oss/extensions"
install_editor code-oss "$HOME/.vscode-oss/extensions"

if ! command -v code >/dev/null 2>&1 && ! command -v codium >/dev/null 2>&1 && ! command -v code-oss >/dev/null 2>&1; then
  say "No VS Code or VSCodium CLI was found. The compiler is installed; install the extension manually from:"
  printf '  %s\n' "$INSTALL_DIR/editors/vscode"
fi

say "Installation complete."
printf 'Compiler: %s/ylang\n' "$BIN_DIR"
printf 'Source:   %s\n' "$INSTALL_DIR"
printf 'Next: restart VS Code/VSCodium, open a .yl file, then use Run Code (Ctrl+Alt+N).\n'

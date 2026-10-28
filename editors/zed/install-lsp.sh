#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
PREFIX="${PREFIX:-$HOME/.local}"
BIN_DIR="$PREFIX/bin"
SHARE_DIR="$PREFIX/share/ylang"
mkdir -p "$BIN_DIR" "$SHARE_DIR/editors/zed"

if [ ! -x "$ROOT/build/ylang" ]; then
    echo "Building the YLang compiler..."
    make -C "$ROOT"
fi

install -m 755 "$ROOT/build/ylang" "$BIN_DIR/ylang"
install -m 644 "$ROOT/editors/zed/ylang-lsp.py" "$SHARE_DIR/editors/zed/ylang-lsp.py"
cat > "$BIN_DIR/ylang-lsp" <<EOF
#!/usr/bin/env sh
export YLANG_COMPILER="$BIN_DIR/ylang"
exec python3 "$SHARE_DIR/editors/zed/ylang-lsp.py" "$@"
EOF
chmod 755 "$BIN_DIR/ylang-lsp"

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *) echo "Add $BIN_DIR to PATH if it is not already present." ;;
esac

echo "Installed YLang compiler and LSP to $PREFIX."
echo "Restart Zed, then open a .yl file."

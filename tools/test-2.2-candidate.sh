#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

NATIVE_CC="${NATIVE_CC:-gcc}"
case "$NATIVE_CC" in
  gcc|clang) ;;
  *) echo "NATIVE_CC must be gcc or clang (got: $NATIVE_CC)" >&2; exit 2 ;;
esac

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    echo "Fedora setup: sudo dnf install gcc clang make python3 pkgconf-pkg-config libcurl-devel gdb nodejs npm" >&2
    echo "Install mdBook 0.5.4 separately; see https://rust-lang.github.io/mdBook/guide/installation.html" >&2
    exit 2
  fi
}

for tool in make python3 pkg-config "$NATIVE_CC" npm node mdbook; do need "$tool"; done
if ! pkg-config --exists libcurl; then
  echo "libcurl development files are required for HTTP tests." >&2
  echo "Fedora: sudo dnf install libcurl-devel pkgconf-pkg-config" >&2
  exit 2
fi
if ! command -v gdb >/dev/null 2>&1 && ! command -v lldb >/dev/null 2>&1; then
  echo "Install GDB (Linux) or LLDB (macOS) to test the debugger path." >&2
  exit 2
fi
NODE_MAJOR="$(node -p 'Number(process.versions.node.split(".")[0])')"
if (( NODE_MAJOR < 22 )); then
  echo "Node.js 22 or newer is required for the website build (found $(node --version))." >&2
  exit 2
fi

step() {
  printf '\n==> %s\n' "$1"
}

step "Build and run the full YLang regression suite (including HTTP/async and debugger tests)"
make clean all CC="$NATIVE_CC" PYTHON=python3
make test CC="$NATIVE_CC" PYTHON=python3

step "Run the repeatable native-backend performance benchmark"
python3 benchmarks/bench-native.py benchmarks/native-loop.yl \
  --cc "$NATIVE_CC" --runs 5 --warmup 1 \
  --expected-file benchmarks/expected-native-loop.txt

step "Run AddressSanitizer and UndefinedBehaviorSanitizer regression tests"
make sanitize CC="$NATIVE_CC" PYTHON=python3

step "Rebuild the normal compiler after sanitizer instrumentation"
make clean all CC="$NATIVE_CC" PYTHON=python3

step "Test the VS Code extension"
npm test --prefix editors/vscode

if command -v nvim >/dev/null 2>&1; then
  step "Test Neovim diagnostics and syntax highlighting"
  nvim --headless -u NONE +'luafile editors/nvim/tests/diagnostics.lua'
  nvim --headless -u NONE +'luafile editors/nvim/tests/syntax.lua'
else
  echo "SKIP: Neovim is not installed; its tests still run in GitHub Actions."
fi

step "Build the online book and validate documentation links"
python3 tools/build_docs_book.py
mdbook build docs/online-book
mkdir -p website/learn
cp -a docs/online-book/book/. website/learn/
python3 tools/check_website.py

step "Build the refreshed React homepage"
npm ci --prefix website-app --no-audit --no-fund
npm run build --prefix website-app
test -s website-app/dist/index.html
test -s website-app/dist/ylang-mark.svg

step "Candidate acceptance checks passed"
echo "Candidate version: $(cat VERSION)"
echo "Commit: $(git rev-parse HEAD 2>/dev/null || echo 'source snapshot without Git metadata')"
echo "No release was published by this script."

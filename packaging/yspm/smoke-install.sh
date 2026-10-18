#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="$(tr -d '\r\n' < "$ROOT/VERSION")"
PACKAGE_PATH="${1:?usage: bash packaging/yspm/smoke-install.sh path/to/package.yspkg}"
YSPM_BIN="${YSPM_BIN:-$ROOT/tools/yspm-source/build/yspm}"

if [[ "$PACKAGE_PATH" != /* ]]; then
    PACKAGE_PATH="$ROOT/$PACKAGE_PATH"
fi
if [[ ! -x "$YSPM_BIN" ]]; then
    echo "smoke-install: cannot execute YSPM at $YSPM_BIN" >&2
    exit 127
fi
if [[ ! -f "$PACKAGE_PATH" ]]; then
    echo "smoke-install: package not found: $PACKAGE_PATH" >&2
    exit 2
fi

PACKAGE_ROOT="$(mktemp -d)"
INSTALL_ROOT="$(mktemp -d)"
cleanup() {
    rm -rf -- "$PACKAGE_ROOT" "$INSTALL_ROOT"
}
trap cleanup EXIT

tar -xzf "$PACKAGE_PATH" -C "$PACKAGE_ROOT"
PACKAGED_COMPILER="$PACKAGE_ROOT/root/usr/bin/ylang"
if [[ ! -x "$PACKAGED_COMPILER" ]]; then
    echo "smoke-install: package is missing root/usr/bin/ylang" >&2
    exit 1
fi

version_output="$("$PACKAGED_COMPILER" --version)"
printf '%s\n' "$version_output"
[[ "$version_output" == "YLang compiler ${VERSION} ("* ]]

# A non-root YSPM_ROOT exercises system-mode package paths without writing to
# the machine's real /usr, /etc, or /var directories.
YSPM_ROOT="$INSTALL_ROOT" "$YSPM_BIN" install --yes "$PACKAGE_PATH"
INSTALLED_COMPILER="$INSTALL_ROOT/usr/bin/ylang"
if [[ ! -x "$INSTALLED_COMPILER" ]]; then
    echo "smoke-install: system-mode install did not create usr/bin/ylang" >&2
    exit 1
fi

export YLANG_PATH="$INSTALL_ROOT/usr/share/ylang/stdlib"
"$INSTALLED_COMPILER" --version
"$INSTALLED_COMPILER" check "$ROOT/examples/demo.yl"
mkdir -p "$INSTALL_ROOT/tmp"
"$INSTALLED_COMPILER" build "$ROOT/examples/demo.yl" -o "$INSTALL_ROOT/tmp/ylang-smoke"
smoke_output="$("$INSTALL_ROOT/tmp/ylang-smoke")"
printf '%s\n' "$smoke_output"
grep -Fq "Compiler works!" <<< "$smoke_output"

YSPM_ROOT="$INSTALL_ROOT" "$YSPM_BIN" remove --yes ylang
test ! -e "$INSTALL_ROOT/usr/bin/ylang"
test ! -e "$INSTALL_ROOT/usr/share/ylang/stdlib/game2d.yl"

echo "PASS: YSPM package install, compiler smoke test, and removal in isolated root."

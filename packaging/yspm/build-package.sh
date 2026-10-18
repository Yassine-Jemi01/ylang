#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="$(tr -d '\r\n' < "$ROOT/VERSION")"
YSPM_BIN="${YSPM_BIN:-yspm}"
YSPM_ABI="${YSPM_ABI:-yspm-abi-1}"
YSPM_ARCH="${YSPM_ARCH:-amd64}"
CC="${CC:-gcc}"
OUTPUT_DIR="${YSPM_OUTPUT_DIR:-$ROOT/build/ylang-yspm}"
OUTPUT="$OUTPUT_DIR/ylang-$VERSION-$YSPM_ARCH.yspkg"

if ! command -v "$YSPM_BIN" >/dev/null 2>&1; then
    echo "build-yspm-package: cannot find yspm; set YSPM_BIN to its executable" >&2
    exit 127
fi
if [[ ! "$VERSION" =~ ^[A-Za-z0-9][A-Za-z0-9.+_-]*$ ]]; then
    echo "build-yspm-package: VERSION contains unsupported characters: $VERSION" >&2
    exit 2
fi

STAGE="$(mktemp -d)"
cleanup() {
    rm -rf -- "$STAGE"
}
trap cleanup EXIT

CFLAGS="${YLANG_PACKAGE_CFLAGS:--std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -O2}"
make -C "$ROOT" clean all CC="$CC" CFLAGS="$CFLAGS" PREFIX=/usr
make -C "$ROOT" DESTDIR="$STAGE" PREFIX=/usr install

install -d \
    "$STAGE/usr/share/ylang/stdlib" \
    "$STAGE/usr/share/doc/ylang" \
    "$STAGE/usr/share/licenses/ylang" \
    "$STAGE/usr/include/ylang"

# Ship the source-level standard library and optional SDL2 runtime sources.
cp -a "$ROOT/stdlib/." "$STAGE/usr/share/ylang/stdlib/"
install -m 644 "$ROOT/include/ylang/gui.h" "$STAGE/usr/include/ylang/gui.h"
install -m 644 "$ROOT/LICENSE" "$STAGE/usr/share/licenses/ylang/LICENSE"

for file in \
    README.md \
    docs/language-spec.md \
    docs/CHANGELOG-2.0-BETA.md \
    docs/GAME2D.md \
    docs/WINDOWS.md
do
    if [[ -f "$ROOT/$file" ]]; then
        install -m 644 "$ROOT/$file" "$STAGE/usr/share/doc/ylang/$(basename "$file")"
    fi
done

mkdir -p "$OUTPUT_DIR"
"$YSPM_BIN" build \
    --root "$STAGE" \
    --output "$OUTPUT" \
    --name ylang \
    --version "$VERSION" \
    --description "YLang compiler and standard library" \
    --abi "$YSPM_ABI" \
    --arch "$YSPM_ARCH" \
    --license MIT \
    --maintainer "YLang project"

test -s "$OUTPUT"
printf 'Built YSPM package: %s\n' "$OUTPUT"

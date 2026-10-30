#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -O2}
$CC $CFLAGS -Iinclude tests/ir_unit.c src/ir.c -o /tmp/ylang-ir-unit
/tmp/ylang-ir-unit

$CC $CFLAGS -Iinclude -Isrc tests/ir_lower_unit.c src/ir_lower.c src/ir.c -o /tmp/ylang-ir-lower-unit
/tmp/ylang-ir-lower-unit

#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -O2}
$CC $CFLAGS -Iinclude tests/ir_unit.c src/ir.c -o /tmp/ylang-ir-unit
/tmp/ylang-ir-unit

$CC $CFLAGS -Iinclude -Isrc tests/ir_lower_unit.c src/ir_lower.c src/ir.c src/parser.c src/lexer.c src/arena.c src/strbuf.c src/diagnostics.c src/string_utils.c src/sema.c -o /tmp/ylang-ir-lower-unit
/tmp/ylang-ir-lower-unit

$CC $CFLAGS -Iinclude tests/ir_print_unit.c src/ir_print.c -o /tmp/ylang-ir-print-unit
/tmp/ylang-ir-print-unit

$CC $CFLAGS -Iinclude tests/ir_c_unit.c src/ir_c.c src/ir.c -o /tmp/ylang-ir-c-unit
/tmp/ylang-ir-c-unit

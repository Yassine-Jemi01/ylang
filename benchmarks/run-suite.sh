#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
COMPILER="${YLANG_COMPILER:-$ROOT/build/ylang}"
NATIVE_CC="${NATIVE_CC:-gcc}"
RUNS="${RUNS:-30}"
WARMUP="${WARMUP:-3}"

if [[ "$NATIVE_CC" != "gcc" && "$NATIVE_CC" != "clang" ]]; then
    echo "NATIVE_CC must be gcc or clang (got: $NATIVE_CC)" >&2
    exit 2
fi
if [[ ! "$RUNS" =~ ^[1-9][0-9]*$ || ! "$WARMUP" =~ ^[0-9]+$ ]]; then
    echo "RUNS must be positive and WARMUP must be zero or greater." >&2
    exit 2
fi
if [[ ! -x "$COMPILER" ]]; then
    echo "YLang compiler not found or not executable: $COMPILER" >&2
    echo "Build it first with: make" >&2
    exit 2
fi

run_case() {
    local backend="$1"
    local source="$2"
    local expected="$3"
    echo
    echo "=== backend=$backend workload=$source ==="
    python3 "$ROOT/benchmarks/bench-native.py" "$ROOT/benchmarks/$source" \
        --compiler "$COMPILER" \
        --backend "$backend" \
        --cc "$NATIVE_CC" \
        --runs "$RUNS" \
        --warmup "$WARMUP" \
        --expected-file "$ROOT/benchmarks/$expected"
}

for workload in native-loop native-calls; do
    run_case c "$workload.yl" "expected-$workload.txt"
done

if [[ "$(uname -s)" == "Linux" && "$(uname -m)" == "x86_64" ]] &&
   command -v nasm >/dev/null 2>&1 && command -v ld >/dev/null 2>&1; then
    for workload in native-loop native-calls; do
        run_case nasm "$workload.yl" "expected-$workload.txt"
    done
else
    echo
    echo "SKIP: NASM benchmarks require Linux x86-64, nasm, and ld."
fi

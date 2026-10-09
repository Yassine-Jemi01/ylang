#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT"

for file in examples/*.yl; do
    echo "Parsing $file"
    output="$(tree-sitter parse "$file" 2>&1)" || {
        printf '%s\n' "$output"
        exit 1
    }
    if printf '%s\n' "$output" | grep -Eq '\((ERROR|MISSING)([[:space:]]|\))'; then
        printf 'Tree-sitter reported syntax errors in %s:\n%s\n' "$file" "$output"
        exit 1
    fi
done

echo "All Tree-sitter YLang examples parsed without syntax errors."

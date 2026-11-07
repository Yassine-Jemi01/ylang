#!/usr/bin/env python3
"""Prevent Arabic-script text from entering compiler/runtime diagnostics."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARABIC = re.compile(r"[\u0600-\u06ff\u0750-\u077f\u08a0-\u08ff\ufb50-\ufdff\ufe70-\ufeff]")
SOURCE_DIRS = (ROOT / "src", ROOT / "runtime", ROOT / "include")
SOURCE_SUFFIXES = {".c", ".h", ".inc"}


def main() -> int:
    checked = 0
    failures: list[str] = []
    for directory in SOURCE_DIRS:
        if not directory.is_dir():
            continue
        for path in sorted(directory.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
                continue
            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError as error:
                failures.append(f"{path.relative_to(ROOT)}: source is not valid UTF-8 ({error})")
                continue
            checked += 1
            for line_number, line in enumerate(lines, 1):
                if ARABIC.search(line):
                    failures.append(f"{path.relative_to(ROOT)}:{line_number}: Arabic-script text is not allowed")
    if not checked:
        print("FAIL: no compiler/runtime source files were checked", file=sys.stderr)
        return 1
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"PASS: compiler diagnostics remain English ({checked} source files checked)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

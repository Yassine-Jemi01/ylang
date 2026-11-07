#!/usr/bin/env python3
"""Verify unused optional runtime sections are removed by the C preprocessor."""
from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OPTIONAL_SYMBOLS = (
    "yl_json_parse",
    "yl_http_request",
    "yl_tui_init",
    "yl_binary_buffer_new",
    "yl_binary_stream_open",
)
SHARED_SYMBOLS = ("yl_to_byte", "yl_binary_seek")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default="gcc", help="native C compiler command")
    args = parser.parse_args()

    compiler = ROOT / "build" / "ylang"
    if not compiler.is_file():
        compiler = ROOT / "build" / "ylang.exe"
    if not compiler.is_file():
        print(f"FAIL: compiler not found: {compiler}", file=sys.stderr)
        return 1

    source = "tests/runtime-feature-gates/minimal.yl"
    generated = ROOT / "build" / "runtime-feature-gates-minimal.c"
    emit = subprocess.run(
        [str(compiler), "emit-c", source, "-o", str(generated)],
        cwd=ROOT, text=True, capture_output=True, check=False,
    )
    if emit.returncode != 0:
        print(emit.stdout, end="")
        print(emit.stderr, end="", file=sys.stderr)
        return emit.returncode

    command = shlex.split(args.cc) + ["-std=c17", "-E", "-P", "-Iinclude", "-Ibuild", str(generated)]
    preprocess = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
    if preprocess.returncode != 0:
        print(preprocess.stdout, end="")
        print(preprocess.stderr, end="", file=sys.stderr)
        return preprocess.returncode

    output = preprocess.stdout
    present = [symbol for symbol in OPTIONAL_SYMBOLS if symbol in output]
    missing_shared = [symbol for symbol in SHARED_SYMBOLS if symbol not in output]
    if present:
        print("FAIL: unused optional runtime symbols remain after preprocessing: " + ", ".join(present), file=sys.stderr)
        return 1
    if missing_shared:
        print("FAIL: shared runtime helpers were unexpectedly removed: " + ", ".join(missing_shared), file=sys.stderr)
        return 1

    print("PASS: unused JSON/HTTP/TUI/Binary sections are excluded; shared byte helpers remain")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

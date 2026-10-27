#!/usr/bin/env python3
"""Smoke-test the repeatable native backend benchmark without fixed performance targets."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
command = [
    sys.executable,
    str(ROOT / "benchmarks" / "bench-native.py"),
    str(ROOT / "benchmarks" / "native-loop.yl"),
    "--runs", "2",
    "--warmup", "1",
    "--expected-file", str(ROOT / "benchmarks" / "expected-native-loop.txt"),
]
result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, timeout=90)
if result.returncode != 0:
    sys.stderr.write(result.stdout)
    sys.stderr.write(result.stderr)
    raise SystemExit(result.returncode)
for label in ("Compile median:", "Run median:", "Binary size:"):
    if label not in result.stdout:
        raise SystemExit(f"benchmark output missing {label!r}:\n{result.stdout}")
print("YLang native benchmark smoke test passed.")

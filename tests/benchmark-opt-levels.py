#!/usr/bin/env python3
"""Compare repeatable native benchmark results with compiler optimization off/on."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "benchmarks" / "native-loop.yl"
EXPECTED = ROOT / "benchmarks" / "expected-native-loop.txt"
BENCHMARK = ROOT / "benchmarks" / "bench-native.py"
reports = {}

for level in ("0", "2"):
    env = os.environ.copy()
    env["YLANG_OPT_LEVEL"] = level
    command = [
        sys.executable, str(BENCHMARK), str(SOURCE),
        "--runs", "2", "--warmup", "1",
        "--expected-file", str(EXPECTED),
    ]
    result = subprocess.run(
        command, cwd=ROOT, env=env, text=True,
        capture_output=True, timeout=90,
    )
    if result.returncode != 0:
        sys.stderr.write(f"Optimization level {level} benchmark failed.\\n")
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    if f"YLANG_OPT_LEVEL: {level}" not in result.stdout:
        raise SystemExit(f"benchmark did not report optimization level {level}")
    required = ("Compile median:", "Run median:", "Binary size:")
    if any(label not in result.stdout for label in required):
        raise SystemExit(f"incomplete benchmark report for optimization level {level}")
    reports[level] = result.stdout

print("YLang optimization comparison smoke test passed (O0 vs O2).")
print("No universal performance threshold is enforced; compare reports on the same host.")

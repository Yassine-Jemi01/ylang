#!/usr/bin/env python3
"""Smoke-test every reproducible native benchmark workload."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORKLOADS = (
    ("native-loop.yl", "expected-native-loop.txt"),
    ("native-branches.yl", "expected-native-branches.txt"),
    ("native-calls.yl", "expected-native-calls.txt"),
    ("native-f64.yl", "expected-native-f64.txt"),
)

for source, expected in WORKLOADS:
    command = [
        sys.executable,
        str(ROOT / "benchmarks" / "bench-native.py"),
        str(ROOT / "benchmarks" / source),
        "--runs", "2",
        "--warmup", "1",
        "--expected-file", str(ROOT / "benchmarks" / expected),
    ]
    result = subprocess.run(
        command, cwd=ROOT, text=True, capture_output=True, timeout=90
    )
    if result.returncode != 0:
        sys.stderr.write(f"Benchmark workload failed: {source}\\n")
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    for label in (
        "Compile median:", "Compile P95:", "Run median:", "Run P95:",
        "Binary size:", "Native compiler command:",
    ):
        if label not in result.stdout:
            raise SystemExit(
                f"{source}: benchmark output missing {label!r}:\\n"
                f"{result.stdout}"
            )
    print(f"YLang native benchmark passed: {source}")

print(f"YLang native benchmark smoke test passed ({len(WORKLOADS)} workloads).")

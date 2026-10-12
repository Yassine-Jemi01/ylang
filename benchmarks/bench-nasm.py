#!/usr/bin/env python3
"""Measure end-to-end launch + execution latency for a compiled NASM binary.

stdout/stderr are redirected so terminal rendering does not dominate the result.
The measurement still includes process creation and exit; it is not a pure CPU
instruction timer and results vary by machine and system load.
"""

from __future__ import annotations

import argparse
import math
import os
import statistics
import subprocess
import sys
import time
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Benchmark a compiled program's process-level latency."
    )
    parser.add_argument("program", type=Path, help="path to the executable to benchmark")
    parser.add_argument("--runs", type=int, default=1000, help="measured runs (default: 1000)")
    parser.add_argument("--warmup", type=int, default=25, help="warm-up runs (default: 25)")
    parser.add_argument(
        "--target-ms",
        type=float,
        default=1.0,
        help="latency target for the median in milliseconds (default: 1.0)",
    )
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be at least 1")
    if args.warmup < 0:
        parser.error("--warmup must be zero or greater")
    if args.target_ms <= 0:
        parser.error("--target-ms must be greater than zero")
    return args


def run_once(program: Path) -> int:
    completed = subprocess.run(
        [str(program)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.returncode


def pretty(ns: int) -> str:
    return f"{ns / 1_000_000:.3f} ms ({ns / 1_000:.1f} µs)"


def main() -> int:
    args = parse_args()
    try:
        program = args.program.resolve(strict=True)
    except FileNotFoundError:
        print(f"error: executable not found: {args.program}", file=sys.stderr)
        return 2

    if not program.is_file() or not os.access(program, os.X_OK):
        print(f"error: not an executable file: {program}", file=sys.stderr)
        return 2

    for _ in range(args.warmup):
        status = run_once(program)
        if status != 0:
            print(f"error: warm-up run exited with status {status}", file=sys.stderr)
            return 1

    samples: list[int] = []
    for _ in range(args.runs):
        start = time.perf_counter_ns()
        status = run_once(program)
        elapsed = time.perf_counter_ns() - start
        if status != 0:
            print(f"error: measured run exited with status {status}", file=sys.stderr)
            return 1
        samples.append(elapsed)

    ordered = sorted(samples)
    p95 = ordered[max(0, math.ceil(0.95 * len(ordered)) - 1)]
    median = statistics.median(samples)
    mean = statistics.fmean(samples)
    target_ns = args.target_ms * 1_000_000

    print(f"Program: {program}")
    print(f"Samples: {args.runs} (after {args.warmup} warm-up runs)")
    print(f"Minimum: {pretty(ordered[0])}")
    print(f"Median:  {pretty(round(median))}")
    print(f"Mean:    {pretty(round(mean))}")
    print(f"P95:     {pretty(p95)}")
    print(
        f"Target:  median < {args.target_ms:.3f} ms — "
        + ("PASS" if median < target_ns else "NOT MET")
    )
    print("Note: includes process launch/exit; stdout and stderr are discarded.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

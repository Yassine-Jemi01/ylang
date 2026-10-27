#!/usr/bin/env python3
"""Reproducible end-to-end benchmark for the YLang C backend.

Measures compile latency, executable launch+runtime latency, and binary size.
It reports measurements instead of imposing a universal speed target: compare
runs on the same machine, C compiler, optimization level, and workload.
"""
from __future__ import annotations

import argparse
import math
import os
import platform
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def percentile(values: list[int], fraction: float) -> int:
    return sorted(values)[max(0, math.ceil(fraction * len(values)) - 1)]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="YLang source file to compile and run")
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "ylang", help="YLang compiler executable")
    parser.add_argument("--cc", choices=("gcc", "clang"), default="gcc", help="native C compiler")
    parser.add_argument("--runs", type=int, default=10, help="measured repetitions (default: 10)")
    parser.add_argument("--warmup", type=int, default=2, help="warm-up repetitions (default: 2)")
    parser.add_argument("--expected-output", help="optional exact stdout expected from the program")
    parser.add_argument("--keep-dir", type=Path, help="optional directory to keep the final binary and generated build artifacts")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be at least one")
    if args.warmup < 0:
        parser.error("--warmup must be zero or greater")
    return args


def run(command: list[str], *, cwd: Path, timeout: float = 30.0) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f"command failed: {' '.join(command)}: {exc}") from exc
    if result.returncode != 0:
        raise RuntimeError(
            f"command exited {result.returncode}: {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def median_ms(samples: list[int]) -> float:
    return statistics.median(samples) / 1_000_000.0


def main() -> int:
    args = parse_args()
    source = args.source.resolve()
    compiler = args.compiler.resolve()
    if not source.is_file():
        print(f"error: source not found: {source}", file=sys.stderr)
        return 2
    if not compiler.is_file() or not os.access(compiler, os.X_OK):
        print(f"error: compiler executable not found: {compiler}", file=sys.stderr)
        return 2

    keep = args.keep_dir.resolve() if args.keep_dir else None
    if keep:
        keep.mkdir(parents=True, exist_ok=True)
        work = keep
        cleanup = False
        temp = None
    else:
        temp = tempfile.TemporaryDirectory(prefix="ylang-bench-")
        work = Path(temp.name)
        cleanup = True

    try:
        executable = work / ("bench.exe" if os.name == "nt" else "bench")
        compile_samples: list[int] = []
        run_samples: list[int] = []
        for iteration in range(args.warmup + args.runs):
            start = time.perf_counter_ns()
            run([str(compiler), "build", str(source), "-o", str(executable), "--cc", args.cc], cwd=ROOT)
            compile_elapsed = time.perf_counter_ns() - start
            check_start = time.perf_counter_ns()
            result = run([str(executable)], cwd=source.parent)
            run_elapsed = time.perf_counter_ns() - check_start
            if args.expected_output is not None and result.stdout != args.expected_output:
                raise RuntimeError(
                    f"output mismatch on iteration {iteration + 1}\n"
                    f"expected: {args.expected_output!r}\nactual:   {result.stdout!r}"
                )
            if iteration >= args.warmup:
                compile_samples.append(compile_elapsed)
                run_samples.append(run_elapsed)

        print("YLang native backend benchmark")
        version_output = run([str(compiler), "--version"], cwd=ROOT).stdout.strip()
        native_cc_version = run([args.cc, "--version"], cwd=ROOT).stdout.splitlines()[0]
        print(f"YLang version: {version_output}")
        print(f"Host: {platform.platform()}")
        print(f"Native C compiler: {native_cc_version}")
        print(f"YLANG_OPT_LEVEL: {os.environ.get('YLANG_OPT_LEVEL', 'default')}")
        print(f"Source: {source}")
        print(f"Measured runs: {args.runs}; warm-ups: {args.warmup}")
        print(f"Compile median: {median_ms(compile_samples):.3f} ms")
        print(f"Compile P95: {percentile(compile_samples, 0.95) / 1_000_000.0:.3f} ms")
        print(f"Run median: {median_ms(run_samples):.3f} ms")
        print(f"Run P95: {percentile(run_samples, 0.95) / 1_000_000.0:.3f} ms")
        print(f"Binary size: {(work / ('bench.exe' if os.name == 'nt' else 'bench')).stat().st_size} bytes")
        print("Note: process startup/exit is included in the run measurement; compare only like-for-like runs.")
        return 0
    except RuntimeError as exc:
        print(f"benchmark error: {exc}", file=sys.stderr)
        return 1
    finally:
        if cleanup and temp is not None:
            temp.cleanup()


if __name__ == "__main__":
    raise SystemExit(main())

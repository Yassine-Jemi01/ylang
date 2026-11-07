#!/usr/bin/env python3
"""Compare a candidate YLang compiler with a stable Git tag on the same host."""
from __future__ import annotations

import argparse
import platform
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def host_cpu() -> str:
    reported = platform.processor().strip()
    if reported:
        return reported
    if sys.platform.startswith("linux"):
        try:
            for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
                if line.lower().startswith("model name") and ":" in line:
                    return line.split(":", 1)[1].strip()
        except OSError:
            pass
    if sys.platform == "darwin":
        try:
            result = subprocess.run(
                ["sysctl", "-n", "machdep.cpu.brand_string"],
                text=True, capture_output=True, check=False,
            )
            if result.returncode == 0 and result.stdout.strip():
                return result.stdout.strip()
        except OSError:
            pass
    return platform.machine() or "unknown"


BENCHMARK = ROOT / "benchmarks" / "bench-native.py"
SOURCE = ROOT / "benchmarks" / "native-loop.yl"
EXPECTED = ROOT / "benchmarks" / "expected-native-loop.txt"
HEAVY_SOURCE = ROOT / "benchmarks" / "native-loop-heavy.yl"
HEAVY_EXPECTED = ROOT / "benchmarks" / "expected-native-loop-heavy.txt"


def run(command: list[str], *, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def metric(report: str, label: str) -> float:
    match = re.search(rf"^{re.escape(label)}: ([0-9.]+)", report, re.MULTILINE)
    if not match:
        raise RuntimeError(f"benchmark report is missing {label}")
    return float(match.group(1))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, help="stable Git tag, for example v2.2.0")
    parser.add_argument("--cc", choices=("gcc", "clang"), default="gcc")
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--warmup", type=int, default=3)
    args = parser.parse_args()
    if args.runs < 5 or args.warmup < 1:
        parser.error("release comparisons require at least 5 measured runs and 1 warm-up")

    output = ROOT / "build" / "release-benchmark"
    output.mkdir(parents=True, exist_ok=True)
    baseline_root = ROOT / "build" / "benchmark-baseline-source"
    if baseline_root.exists():
        raise RuntimeError(f"baseline worktree path already exists: {baseline_root}")

    candidate_sha = run(["git", "rev-parse", "HEAD"]).stdout.strip()
    run(["git", "fetch", "--no-tags", "origin",
         f"refs/tags/{args.baseline}:refs/tags/{args.baseline}"])
    run(["git", "worktree", "add", "--detach", str(baseline_root), args.baseline])
    run(["make", "clean", "all", f"CC={args.cc}"], cwd=baseline_root)

    def measure(label: str, compiler: Path, source: Path, expected: Path, suffix: str) -> str:
        artifact_dir = output / f"{label}-{suffix}-artifacts"
        command = [
            sys.executable, str(BENCHMARK), str(source),
            "--compiler", str(compiler), "--cc", args.cc,
            "--runs", str(args.runs), "--warmup", str(args.warmup),
            "--expected-file", str(expected), "--keep-dir", str(artifact_dir),
        ]
        result = run(command)
        report = (
            f"Candidate commit: {candidate_sha}\n"
            f"Stable baseline tag: {args.baseline}\n"
            f"Benchmark compiler role: {label}\n"
            f"Workload: {suffix}\n\n{result.stdout}"
        )
        (output / f"{label}-{suffix}.txt").write_text(report, encoding="utf-8")
        return result.stdout

    candidate_compact = measure("candidate", ROOT / "build" / "ylang", SOURCE, EXPECTED, "compact")
    baseline_compact = measure("baseline", baseline_root / "build" / "ylang", SOURCE, EXPECTED, "compact")
    candidate_heavy = measure("candidate", ROOT / "build" / "ylang", HEAVY_SOURCE, HEAVY_EXPECTED, "10m-loop")
    baseline_heavy = measure("baseline", baseline_root / "build" / "ylang", HEAVY_SOURCE, HEAVY_EXPECTED, "10m-loop")
    fields = (
        ("Compile median", "Compile median", "ms", candidate_compact, baseline_compact),
        ("Compile P95", "Compile P95", "ms", candidate_compact, baseline_compact),
        ("Launch + short run median", "Run median", "ms", candidate_compact, baseline_compact),
        ("10M loop run median", "Run median", "ms", candidate_heavy, baseline_heavy),
        ("10M loop run P95", "Run P95", "ms", candidate_heavy, baseline_heavy),
        ("Binary size", "Binary size", "bytes", candidate_compact, baseline_compact),
    )
    summary = [
        "YLang candidate versus stable baseline",
        f"Candidate commit: {candidate_sha}",
        f"Baseline tag: {args.baseline}",
        f"Host: {platform.platform()}",
        f"Architecture: {platform.machine() or 'unknown'}",
        f"CPU: {host_cpu()}",
        f"Native C compiler: {args.cc}",
        f"Measured runs: {args.runs}; warm-ups: {args.warmup}",
        "",
        "Ratios are candidate / baseline; below 1.0 means the candidate was lower for that measurement.",
    ]
    for label, metric_label, unit, candidate_report, baseline_report in fields:
        current = metric(candidate_report, metric_label)
        baseline = metric(baseline_report, metric_label)
        ratio = current / baseline if baseline else float("inf")
        summary.append(
            f"{label}: candidate={current:.3f} {unit}; baseline={baseline:.3f} {unit}; ratio={ratio:.3f}"
        )
    summary.append("")
    summary.append("No universal performance threshold is enforced; compare only this paired host/toolchain/workload run.")
    text = "\n".join(summary) + "\n"
    (output / "comparison.txt").write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as error:
        print(f"benchmark comparison failed: {error}", file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Paired long-running memory-retention benchmark for YLang 2.3 and C.

The four cases deliberately separate lifetime policy from language:
- YLang legacy string vs C retained allocations
- YLang owned_string vs C malloc/free

RSS is sampled while each process is alive. YLang's tracked-byte diagnostic is
also recorded at normal exit; it is not treated as RSS.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAYLOAD_BYTES = 256


def read_rss_kib(pid: int) -> int | None:
    try:
        for line in Path(f"/proc/{pid}/status").read_text(encoding="ascii").splitlines():
            if line.startswith("VmRSS:"):
                return int(line.split()[1])
    except (OSError, ValueError):
        return None
    return None


def cpu_name() -> str:
    try:
        for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
            if line.lower().startswith("model name") and ":" in line:
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def run_sample(label: str, command: list[str], env: dict[str, str],
               interval: float, output_dir: Path) -> dict[str, object]:
    samples: list[tuple[float, int]] = []
    started = time.monotonic()
    process = subprocess.Popen(command, cwd=ROOT, env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    while process.poll() is None:
        rss = read_rss_kib(process.pid)
        if rss is not None:
            samples.append((time.monotonic() - started, rss))
        time.sleep(interval)
    stdout, stderr = process.communicate()
    elapsed = time.monotonic() - started
    if process.returncode != 0:
        raise RuntimeError(
            f"{label} exited {process.returncode}\nstdout:\n{stdout}\nstderr:\n{stderr}"
        )
    csv_path = output_dir / f"{label}.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("elapsed_seconds", "rss_kib"))
        writer.writerows((f"{t:.3f}", rss) for t, rss in samples)

    rss_values = [value for _, value in samples]
    stats_match = re.search(
        r"YLang memory stats: tracked_allocations=(\d+) tracked_bytes=(\d+)", stderr
    )
    result: dict[str, object] = {
        "case": label,
        "elapsed_seconds": round(elapsed, 3),
        "samples": len(samples),
        "rss_start_mib": round(rss_values[0] / 1024, 2) if rss_values else None,
        "rss_peak_mib": round(max(rss_values) / 1024, 2) if rss_values else None,
        "rss_last_sample_mib": round(rss_values[-1] / 1024, 2) if rss_values else None,
        "rss_growth_mib": round((max(rss_values) - rss_values[0]) / 1024, 2) if rss_values else None,
        "tracked_allocations_at_exit": int(stats_match.group(1)) if stats_match else None,
        "tracked_bytes_at_exit": int(stats_match.group(2)) if stats_match else None,
        "stdout": stdout.strip(),
        "stderr_tail": stderr.strip().splitlines()[-1] if stderr.strip() else "",
        "samples_file": csv_path.name,
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "ylang")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--iterations", type=int, default=2_000_000)
    parser.add_argument("--interval", type=float, default=0.1)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "memory-soak")
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("RSS sampling currently requires Linux /proc")
    if args.iterations < 1 or args.interval <= 0:
        parser.error("iterations and interval must be positive")

    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise RuntimeError(f"YLang compiler not found: {compiler}")
    output_dir = args.output.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    temp_dir = output_dir / "sources"
    temp_dir.mkdir(exist_ok=True)

    cases: list[tuple[str, list[str], dict[str, str]]] = []
    base_env = os.environ.copy()
    base_env["YLANG_OPT_LEVEL"] = "2"

    for label, source_name in (
        ("ylang-legacy", "memory-soak.yl"),
        ("ylang-owned", "memory-soak-owned.yl"),
    ):
        template = (ROOT / "benchmarks" / source_name).read_text(encoding="utf-8")
        source = temp_dir / source_name
        source.write_text(template.replace("__ITERATIONS__", str(args.iterations)), encoding="utf-8")
        binary = temp_dir / label
        subprocess.run(
            [str(compiler), "build", str(source), "-o", str(binary), "--cc", args.cc],
            cwd=ROOT, env=base_env, check=True, text=True,
        )
        env = base_env.copy()
        env["YLANG_MEMORY_STATS"] = "1"
        cases.append((label, [str(binary)], env))

    c_binary = temp_dir / "memory-soak-c"
    subprocess.run(
        [args.cc, "-std=c17", "-O2", "-Wall", "-Wextra", "-Wpedantic",
         str(ROOT / "benchmarks" / "memory-soak.c"), "-o", str(c_binary)],
        cwd=ROOT, check=True, text=True,
    )
    cases.extend((
        ("c-retained", [str(c_binary), str(args.iterations), "retain"], base_env),
        ("c-freed", [str(c_binary), str(args.iterations), "free"], base_env),
    ))

    results = []
    for label, command, env in cases:
        print(f"Running {label}...", flush=True)
        result = run_sample(label, command, env, args.interval, output_dir)
        results.append(result)
        print(json.dumps(result, sort_keys=True), flush=True)

    metadata = {
        "host": platform.platform(),
        "architecture": platform.machine(),
        "cpu": cpu_name(),
        "native_compiler": subprocess.run(
            [args.cc, "--version"], capture_output=True, text=True, check=True
        ).stdout.splitlines()[0],
        "ylang_version": subprocess.run(
            [str(compiler), "--version"], capture_output=True, text=True, check=True
        ).stdout.strip(),
        "iterations_per_case": args.iterations,
        "payload_bytes_per_allocation": PAYLOAD_BYTES,
        "sample_interval_seconds": args.interval,
        "optimization": "-O2",
        "results": results,
    }
    (output_dir / "results.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )

    rows = [
        "# YLang 2.3 Memory Soak Benchmark",
        "",
        f"- Host: {metadata['host']}",
        f"- CPU: {metadata['cpu']}",
        f"- Native compiler: {metadata['native_compiler']}",
        f"- YLang: {metadata['ylang_version']}",
        f"- Workload: {args.iterations:,} allocations per case, {PAYLOAD_BYTES} payload bytes each",
        f"- RSS sampling interval: {args.interval:g}s",
        "- Optimization: -O2",
        "",
        "| Case | Time (s) | RSS start (MiB) | RSS peak (MiB) | RSS growth (MiB) | Tracked allocations at exit | Tracked bytes at exit |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for result in results:
        rows.append(
            "| {case} | {elapsed_seconds} | {rss_start_mib} | {rss_peak_mib} | "
            "{rss_growth_mib} | {tracked_allocations_at_exit} | {tracked_bytes_at_exit} |".format(**result)
        )
    rows.extend((
        "",
        "## Interpretation",
        "",
        "The retained cases intentionally keep every allocation alive until process exit. "
        "The freed cases release each allocation immediately. Compare YLang legacy with C retained, "
        "and YLang owned_string with C malloc/free; do not compare unlike lifetime policies.",
        "",
        "RSS is sampled from /proc/<pid>/status. The final RSS sample is taken before process exit; "
        "the allocator may keep freed pages mapped, so RSS need not fall immediately after free. "
        "YLang tracked bytes are a separate runtime counter sampled at normal exit and exclude allocator "
        "metadata. This is a retention stress test, not a general language-speed benchmark.",
        "",
        "Raw time-series samples are in the accompanying CSV files.",
        "",
    ))
    report = "\n".join(rows)
    (output_dir / "report.md").write_text(report, encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"memory benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1)

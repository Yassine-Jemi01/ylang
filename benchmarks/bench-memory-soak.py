#!/usr/bin/env python3
"""Long-duration, paired memory-retention soak for YLang 2.3 and C."""
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


def expect_ready(process: subprocess.Popen[str], label: str) -> None:
    assert process.stdout is not None
    line = process.stdout.readline()
    if line != "READY\n":
        stderr = process.stderr.read() if process.stderr else ""
        raise RuntimeError(f"{label} did not reach the next batch boundary; output={line!r}; stderr={stderr}")


def run_case(label: str, command: list[str], env: dict[str, str], rounds: int,
             pause: float, interval: float, output_dir: Path) -> dict[str, object]:
    process = subprocess.Popen(command, cwd=ROOT, env=env, text=True, bufsize=1,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    assert process.stdin is not None
    samples: list[tuple[float, int, int]] = []
    started = time.monotonic()

    def sample_window(round_number: int) -> None:
        deadline = time.monotonic() + pause
        while True:
            rss = read_rss_kib(process.pid)
            if rss is not None:
                samples.append((time.monotonic() - started, round_number, rss))
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            time.sleep(min(interval, remaining))

    try:
        expect_ready(process, label)
        sample_window(0)
        for round_number in range(1, rounds + 1):
            process.stdin.write("1\n")
            process.stdin.flush()
            expect_ready(process, label)
            sample_window(round_number)
        process.stdin.write("0\n")
        process.stdin.flush()
        stdout, stderr = process.communicate(timeout=30)
        elapsed = time.monotonic() - started
    except Exception:
        process.kill()
        process.communicate()
        raise

    if process.returncode != 0:
        raise RuntimeError(f"{label} exited {process.returncode}\nstdout:\n{stdout}\nstderr:\n{stderr}")

    csv_path = output_dir / f"{label}.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("elapsed_seconds", "completed_round", "rss_kib"))
        writer.writerows((f"{t:.3f}", round_number, rss) for t, round_number, rss in samples)

    rss_values = [value for _, _, value in samples]
    per_round = []
    for round_number in range(rounds + 1):
        values = [rss for _, r, rss in samples if r == round_number]
        per_round.append({
            "round": round_number,
            "rss_median_mib": round(sum(values) / len(values) / 1024, 2) if values else None,
            "rss_peak_mib": round(max(values) / 1024, 2) if values else None,
        })
    stats_match = re.search(
        r"YLang memory stats: tracked_allocations=(\d+) tracked_bytes=(\d+)", stderr
    )
    return {
        "case": label,
        "elapsed_seconds": round(elapsed, 2),
        "samples": len(samples),
        "rss_start_mib": round(rss_values[0] / 1024, 2) if rss_values else None,
        "rss_peak_mib": round(max(rss_values) / 1024, 2) if rss_values else None,
        "rss_last_sample_mib": round(rss_values[-1] / 1024, 2) if rss_values else None,
        "rss_growth_mib": round((rss_values[-1] - rss_values[0]) / 1024, 2) if rss_values else None,
        "tracked_allocations_at_exit": int(stats_match.group(1)) if stats_match else None,
        "tracked_bytes_at_exit": int(stats_match.group(2)) if stats_match else None,
        "stdout": stdout.strip(),
        "stderr_tail": stderr.strip().splitlines()[-1] if stderr.strip() else "",
        "samples_file": csv_path.name,
        "per_round": per_round,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "ylang")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--batch", type=int, default=100_000)
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--pause", type=float, default=15.0)
    parser.add_argument("--interval", type=float, default=0.5)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "memory-soak")
    args = parser.parse_args()
    if sys.platform != "linux":
        parser.error("RSS sampling currently requires Linux /proc")
    if args.batch < 1 or args.rounds < 1 or args.pause <= 0 or args.interval <= 0:
        parser.error("batch, rounds, pause, and interval must be positive")

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
        source.write_text(template.replace("__BATCH__", str(args.batch)), encoding="utf-8")
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
        ("c-retained", [str(c_binary), str(args.batch), "retain"], base_env),
        ("c-freed", [str(c_binary), str(args.batch), "free"], base_env),
    ))

    results = []
    for label, command, env in cases:
        print(f"Starting {label}: {args.rounds} rounds, {args.batch:,} allocations/round, "
              f"{args.pause:g}s observation after each round", flush=True)
        result = run_case(label, command, env, args.rounds, args.pause, args.interval, output_dir)
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
        "rounds": args.rounds,
        "allocations_per_round": args.batch,
        "total_allocations_per_case": args.rounds * args.batch,
        "payload_bytes_per_allocation": PAYLOAD_BYTES,
        "pause_after_each_round_seconds": args.pause,
        "sample_interval_seconds": args.interval,
        "optimization": "-O2",
        "results": results,
    }
    (output_dir / "results.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    rows = [
        "# YLang 2.3 Long-Duration Memory Soak",
        "",
        f"- Host: {metadata['host']}",
        f"- CPU: {metadata['cpu']}",
        f"- Native compiler: {metadata['native_compiler']}",
        f"- YLang: {metadata['ylang_version']}",
        f"- Workload: {args.rounds} rounds × {args.batch:,} allocations = {args.rounds * args.batch:,} allocations per case",
        f"- Payload: {PAYLOAD_BYTES} bytes per string",
        f"- Observation: {args.pause:g}s after each round; RSS sampled every {args.interval:g}s",
        "- Optimization: -O2",
        "",
        "| Case | Duration (s) | RSS baseline (MiB) | RSS peak (MiB) | RSS change (MiB) | Tracked allocations at exit | Tracked bytes at exit |",
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
        "Compare YLang legacy string with C retained allocations, and YLang owned_string with C malloc/free. "
        "Each process stays alive through all rounds; the harness pauses after each batch and samples RSS "
        "while the process is idle, so the report shows whether memory rises between rounds or remains bounded.",
        "",
        "RSS comes from /proc/<pid>/status. Allocators may keep freed pages mapped, so a flat RSS after free "
        "does not prove that live allocations remain. YLang tracked-byte counters are reported separately "
        "at normal process exit and exclude allocator metadata.",
        "",
        "Raw per-sample time series and per-round summaries are included as CSV and JSON.",
        "",
    ))
    report = "\n".join(rows)
    (output_dir / "report.md").write_text(report, encoding="utf-8")
    print(report)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"memory benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1)

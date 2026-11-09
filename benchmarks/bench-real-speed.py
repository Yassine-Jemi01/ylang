#!/usr/bin/env python3
"""Paired YLang 2.3 vs C execution benchmarks on one host and toolchain."""
from __future__ import annotations
import argparse, csv, json, math, os, platform, statistics, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUITE = ROOT / "benchmarks" / "real-speed"

def run(command, env, timeout=180):
    result = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True,
                            timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}\n"
                           f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
    return result

def percentile(values, p):
    return sorted(values)[max(0, math.ceil(p * len(values)) - 1)]

def cpu_name():
    if sys.platform.startswith("linux"):
        try:
            for line in Path("/proc/cpuinfo").read_text(encoding="utf-8").splitlines():
                if line.lower().startswith("model name") and ":" in line:
                    return line.split(":", 1)[1].strip()
        except OSError:
            pass
    return platform.processor() or platform.machine() or "unknown"

def measure_pair(ylang_bin, c_bin, expected, runs, warmup, env):
    commands = {"ylang": [str(ylang_bin)], "c": [str(c_bin)]}
    samples = {"ylang": [], "c": []}
    for index in range(warmup + runs):
        order = ("ylang", "c") if index % 2 == 0 else ("c", "ylang")
        for label in order:
            started = time.perf_counter_ns()
            result = run(commands[label], env)
            elapsed = time.perf_counter_ns() - started
            if result.stdout != expected:
                raise RuntimeError(f"{label} output mismatch: expected {expected!r}, got {result.stdout!r}")
            if index >= warmup:
                samples[label].append(elapsed)
    y, c = samples["ylang"], samples["c"]
    y_med, c_med = statistics.median(y), statistics.median(c)
    return {
        "ylang_median_ms": round(y_med / 1e6, 3),
        "c_median_ms": round(c_med / 1e6, 3),
        "ylang_p95_ms": round(percentile(y, .95) / 1e6, 3),
        "c_p95_ms": round(percentile(c, .95) / 1e6, 3),
        "ylang_over_c": round(y_med / c_med, 4) if c_med else None,
        "ylang_samples_ms": [round(x / 1e6, 4) for x in y],
        "c_samples_ms": [round(x / 1e6, 4) for x in c],
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "ylang")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "real-speed")
    args = parser.parse_args()
    if args.runs < 10 or args.warmup < 2:
        parser.error("use at least 10 measured runs and 2 warm-ups")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise RuntimeError(f"YLang compiler not found: {compiler}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    bins = output / "bin"
    bins.mkdir(exist_ok=True)
    env = os.environ.copy()
    env["YLANG_OPT_LEVEL"] = "2"
    cflags = ["-std=c17", "-Wall", "-Wextra", "-Wpedantic",
              "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
              "-O2", "-g", "-D_FORTIFY_SOURCE=3", "-fstack-protector-strong",
              "-fPIE", "-pie", "-Wl,-z,relro,-z,now"]
    results = {}
    for name in ("arithmetic", "branching", "array", "function-calls"):
        yl_source, c_source = SUITE / f"{name}.yl", SUITE / f"{name}.c"
        yl_bin, c_bin = bins / f"{name}-ylang", bins / f"{name}-c"
        run([str(compiler), "build", str(yl_source), "-o", str(yl_bin), "--cc", args.cc], env)
        run([args.cc, *cflags, str(c_source), "-lm", "-o", str(c_bin)], env)
        expected = run([str(c_bin)], env).stdout
        results[name] = measure_pair(yl_bin, c_bin, expected, args.runs, args.warmup, env)
        print(f"{name}: {json.dumps(results[name], sort_keys=True)}", flush=True)
    cc_version = run([args.cc, "--version"], env).stdout.splitlines()[0]
    ylang_version = run([str(compiler), "--version"], env).stdout.strip()
    metadata = {
        "suite": "YLang 2.3 vs C",
        "ylang_version": ylang_version,
        "ylang_compiler_commit": run(["git", "rev-parse", "HEAD"], env).stdout.strip(),
        "host": platform.platform(), "architecture": platform.machine(), "cpu": cpu_name(),
        "native_c_compiler": cc_version, "optimization": "-O2",
        "runs": args.runs, "warmups": args.warmup,
        "timing": "wall-clock process execution including startup and exit; stdout checked each run",
        "paired_order": "alternating YLang-first and C-first", "c_flags": cflags, "workloads": results,
    }
    (output / "results.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    with (output / "samples.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("workload", "language", "sample", "milliseconds"))
        for name, result in results.items():
            for language in ("ylang", "c"):
                for index, value in enumerate(result[f"{language}_samples_ms"], 1):
                    writer.writerow((name, language, index, value))
    rows = [
        "# YLang 2.3 vs C — Native execution benchmark", "",
        f"- YLang: {ylang_version}", f"- Commit: {metadata['ylang_compiler_commit']}",
        f"- Host: {metadata['host']}", f"- CPU: {metadata['cpu']}",
        f"- Native C compiler: {cc_version}", "- Optimization: -O2 for both",
        f"- Samples: {args.runs} measured + {args.warmup} warm-ups per program",
        "- Timing: process wall-clock time, including launch/exit; output verified every run",
        "- Pairing: execution order alternates to reduce systematic order bias", "",
        "| Workload | YLang median (ms) | C median (ms) | YLang/C | YLang P95 (ms) | C P95 (ms) |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for name, result in results.items():
        rows.append(f"| {name} | {result['ylang_median_ms']} | {result['c_median_ms']} | "
                    f"{result['ylang_over_c']}x | {result['ylang_p95_ms']} | {result['c_p95_ms']} |")
    rows.extend(["", "## Interpreting the ratio", "",
        "- YLang/C = 1.00x: equal median runtime.",
        "- YLang/C > 1.00x: YLang took longer; 1.10x means 10% slower.",
        "- YLang/C < 1.00x: YLang took less time in this sample.", "",
        "Both implementations use the same algorithm, input/iteration counts, native compiler, "
        "and -O2 plus matching hardening flags. This is a C-backend comparison, not an interpreter "
        "comparison. The generated YLang executable includes the YLang runtime; the C executable "
        "is a direct native implementation. Results are specific to this host and toolchain.", "",
        "Workloads cover integer arithmetic, conditional branches, array indexing, and function calls. "
        "They do not represent every YLang feature or real-world application.", "",
        "Raw timings are in samples.csv; full metadata and exact ratios are in results.json.", ""])
    (output / "report.md").write_text("\n".join(rows), encoding="utf-8")
    print("\n".join(rows))
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
        print(f"benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1)

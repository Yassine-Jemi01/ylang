#!/usr/bin/env python3
"""Measure YLang-generated C and direct C inside the same in-process timer."""
from __future__ import annotations
import argparse, csv, json, math, os, platform, re, statistics, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUITE = ROOT / "benchmarks" / "compute-only"
WORKLOADS = ("arithmetic", "branching", "array", "function-calls")

HARNESS = r"""
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static uint64_t bench_now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) { perror("clock_gettime"); exit(70); }
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
int main(int argc, char **argv) {
    if (argc != 3) return 64;
    char *end = NULL;
    long warmups = strtol(argv[1], &end, 10);
    if (!end || *end || warmups < 0 || warmups > 1000) return 64;
    long runs = strtol(argv[2], &end, 10);
    if (!end || *end || runs < 1 || runs > 1000) return 64;
    volatile int64_t sink = 0;
    for (long i = 0; i < warmups; ++i) sink = BENCH_FUNCTION();
    for (long i = 0; i < runs; ++i) {
        uint64_t started = bench_now_ns();
        int64_t checksum = BENCH_FUNCTION();
        uint64_t elapsed = bench_now_ns() - started;
        sink = checksum;
        printf("%" PRIu64 ",%" PRId64 "\n", elapsed, checksum);
    }
#ifdef YLANG_GENERATED
    yl_cleanup();
#endif
    return sink == INT64_MIN ? 1 : 0;
}
"""

def run(command, env, timeout=180):
    result = subprocess.run(command, cwd=ROOT, env=env, text=True,
                            capture_output=True, timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(command)}\n"
                           f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
    return result.stdout

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

def compile_pair(name, compiler, cc, build, env):
    generated = build / f"{name}.generated.c"
    ylang_bin, c_bin = build / f"{name}-ylang", build / f"{name}-c"
    run([str(compiler), "emit-c", str(SUITE / f"{name}.yl"), "-o", str(generated)], env)
    source = generated.read_text(encoding="utf-8")
    match = re.search(r"(?m)^int64_t (yl_fn_m[0-9]+_benchmark)\(void\) \{", source)
    if not match:
        raise RuntimeError(f"could not locate generated benchmark function in {generated}")
    symbol = match.group(1)
    source = re.sub(r"(?m)^int64_t (yl_fn_m[0-9]+_[A-Za-z0-9_]+)(\([^)]*\) \{)",
                      r"__attribute__((noinline)) int64_t \1\2", source)
    source += HARNESS.replace("BENCH_FUNCTION", symbol).replace(
        "#include <inttypes.h>", "#define YLANG_GENERATED 1\n#include <inttypes.h>")
    generated.write_text(source, encoding="utf-8")

    direct = "#define _POSIX_C_SOURCE 200809L\n" + (SUITE / f"{name}.c").read_text(encoding="utf-8")
    direct += HARNESS.replace("BENCH_FUNCTION", "benchmark")
    c_source = build / f"{name}.direct.c"
    c_source.write_text(direct, encoding="utf-8")
    flags = ["-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-function",
             "-Wno-unused-variable", "-Wno-unused-but-set-variable", "-O2", "-g",
             "-D_FORTIFY_SOURCE=3", "-fstack-protector-strong", "-fPIE", "-pie",
             "-Wl,-z,relro,-z,now"]
    run([cc, *flags, str(generated), "-o", str(ylang_bin)], env)
    run([cc, *flags, str(c_source), "-o", str(c_bin)], env)
    return ylang_bin, c_bin

def sample(binary, warmups, env):
    output = run([str(binary), str(warmups), "1"], env).strip()
    lines = output.splitlines()
    if len(lines) != 1 or len(lines[0].split(",")) != 2:
        raise RuntimeError(f"malformed timed output from {binary}: {output!r}")
    elapsed, checksum = lines[0].split(",")
    return int(elapsed), int(checksum)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "ylang")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--warmup", type=int, default=2)
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "compute-only")
    args = parser.parse_args()
    if args.runs < 10 or args.warmup < 1:
        parser.error("use at least 10 samples and 1 warm-up")
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        raise RuntimeError(f"YLang compiler not found: {compiler}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["YLANG_OPT_LEVEL"] = "2"
    results = {}
    with tempfile.TemporaryDirectory(prefix="ylang-compute-only-") as temp:
        build = Path(temp)
        for name in WORKLOADS:
            ybin, cbin = compile_pair(name, compiler, args.cc, build, env)
            times = {"ylang": [], "c": []}
            checksums = {"ylang": [], "c": []}
            for index in range(args.runs):
                order = ("ylang", "c") if index % 2 == 0 else ("c", "ylang")
                for language in order:
                    binary = ybin if language == "ylang" else cbin
                    elapsed, checksum = sample(binary, args.warmup, env)
                    times[language].append(elapsed)
                    checksums[language].append(checksum)
            if checksums["ylang"] != checksums["c"]:
                raise RuntimeError(f"{name}: YLang/C checksum mismatch")
            ym, cm = statistics.median(times["ylang"]), statistics.median(times["c"])
            results[name] = {
                "ylang_median_ms": round(ym / 1e6, 4),
                "c_median_ms": round(cm / 1e6, 4),
                "ylang_p95_ms": round(percentile(times["ylang"], .95) / 1e6, 4),
                "c_p95_ms": round(percentile(times["c"], .95) / 1e6, 4),
                "ylang_over_c": round(ym / cm, 4) if cm else None,
                "checksum": checksums["ylang"][0],
                "ylang_samples_ns": times["ylang"], "c_samples_ns": times["c"],
            }
            print(f"{name}: YLang {results[name]['ylang_median_ms']} ms; "
                  f"C {results[name]['c_median_ms']} ms; "
                  f"ratio {results[name]['ylang_over_c']}x", flush=True)

    cc_version = run([args.cc, "--version"], env).splitlines()[0]
    ylang_version = run([str(compiler), "--version"], env).strip()
    metadata = {
        "suite": "YLang 2.3 compute-only vs C", "ylang_version": ylang_version,
        "commit": run(["git", "rev-parse", "HEAD"], env).strip(),
        "host": platform.platform(), "architecture": platform.machine(),
        "cpu": cpu_name(), "compiler": cc_version, "optimization": "-O2",
        "runs": args.runs, "warmups_per_sample": args.warmup,
        "timer": "CLOCK_MONOTONIC around benchmark() only",
        "excluded": ["process startup", "warm-up", "printing", "YLang cleanup"],
        "paired_order": "alternating YLang-first and C-first",
        "checksums_verified": True, "workloads": results,
    }
    (output / "results.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    with (output / "samples.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("workload", "language", "sample", "nanoseconds", "checksum"))
        for name, result in results.items():
            for language in ("ylang", "c"):
                for i, value in enumerate(result[f"{language}_samples_ns"], 1):
                    writer.writerow((name, language, i, value, result["checksum"]))
    rows = [
        "# YLang 2.3 Compute-only Benchmark", "",
        f"- YLang: {ylang_version}", f"- Commit: {metadata['commit']}",
        f"- Host: {metadata['host']}", f"- CPU: {metadata['cpu']}",
        f"- Compiler: {cc_version}", "- Optimization: -O2 for both",
        f"- Samples: {args.runs} per language/workload",
        f"- Warm-ups: {args.warmup} per process, before timing",
        "- Timer: CLOCK_MONOTONIC around benchmark() only",
        "- Process startup, warm-ups, output, and YLang cleanup are excluded",
        "- Every checksum is compared; mismatches fail the run", "",
        "| Workload | YLang median ms | C median ms | YLang/C | YLang P95 ms | C P95 ms |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for name, r in results.items():
        rows.append(f"| {name} | {r['ylang_median_ms']} | {r['c_median_ms']} | "
                    f"{r['ylang_over_c']}x | {r['ylang_p95_ms']} | {r['c_p95_ms']} |")
    rows += ["", "## Method", "",
        "Each YLang workload is emitted to C with the YLang compiler. The generated "
        "function and direct C function are compiled with the same native compiler "
        "and flags. A shared C harness times only the benchmark function call using "
        "CLOCK_MONOTONIC. The benchmark function is marked noinline in both builds.",
        "", "This measures YLang's generated compute path, including generated runtime "
        "helpers called by that path. It excludes process startup and runtime cleanup. "
        "Results are specific to this host, compiler, flags, and workload.", "",
        "Raw samples: samples.csv. Full metadata: results.json.", ""]
    (output / "report.md").write_text("\n".join(rows), encoding="utf-8")
    print("\n".join(rows))
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired, ValueError) as error:
        print(f"compute-only benchmark failed: {error}", file=sys.stderr)
        raise SystemExit(1)

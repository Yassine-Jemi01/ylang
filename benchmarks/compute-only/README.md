# Compute-only YLang vs C benchmark

This suite measures the execution time of the workload function itself, not the
time to launch a process or print its result.

## Run

From the repository root, after building YLang:

    make clean all CC=gcc
    python3 benchmarks/bench-compute-only.py --cc gcc --runs 30 --warmup 2

The script requires YLang 2.3's emit-c command and GCC or Clang. It writes:

- build/compute-only/report.md
- build/compute-only/results.json
- build/compute-only/samples.csv

## Measurement design

Each YLang workload is emitted to C with ylang emit-c. The generated function
and its direct C equivalent are compiled by the same native compiler with
matching -O2 and hardening flags. A shared C harness calls the workload
function between two CLOCK_MONOTONIC reads. Only that function call is timed.

The harness performs warm-up calls before timing, prints checksums only after
the timed section, and calls YLang cleanup after timing. It marks the workload
function noinline in both binaries to preserve the same call boundary. Each
sample runs in a fresh process, but process startup is outside the measured
interval. Execution order alternates between YLang-first and C-first.

All checksums must match or the benchmark stops with an error. The suite
currently covers integer arithmetic, conditional branching, fixed-array
indexing, and function calls.

## Interpreting results

YLang/C = 1.00x means equal median time. A ratio above 1.00x means YLang took
longer; below 1.00x means YLang took less time in this sample.

This compares YLang's generated C compute path with direct C. Any runtime
helpers or safety checks emitted for YLang operations remain part of the
measured function, because they are part of the generated implementation.
Results are specific to the machine, compiler, flags, and workloads; they
are not a universal language ranking.

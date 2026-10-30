# Reproducible Performance Measurements

**Status: development tooling. Numbers are machine-specific and are not release guarantees.**

## Run the native benchmark suite

Build YLang first, then run the same workloads on the same host and toolchain:

```sh
make
RUNS=30 WARMUP=3 NATIVE_CC=gcc bash benchmarks/run-suite.sh
```

The suite measures the C backend and, on Linux x86-64 when NASM and `ld` are installed, the NASM backend. It uses identical YLang source programs and validates their exact output. The compiler path can be overridden with `YLANG_COMPILER=/path/to/ylang`.

The individual harness is also available:

```sh
python3 benchmarks/bench-native.py benchmarks/native-loop.yl \
  --backend c --cc gcc --runs 30 --warmup 3 \
  --expected-file benchmarks/expected-native-loop.txt
```

Use `--cc clang` to measure the C backend with Clang. The NASM mode is restricted to Linux x86-64 and requires both `nasm` and `ld`. Add `--keep-dir build/benchmark-artifacts` to preserve the final generated executable for inspection.

## Workloads

- **native-loop:** 20 million integer loop iterations with a checksum. This is intentionally large enough to reduce (not eliminate) process-startup noise.
- **native-calls:** 2 million calls across a small integer function. This checks call overhead and basic argument passing.
- Future cases must cover floating-point work, branching, memory access, strings/allocations, and collections before release claims are made.

Both initial workloads use features currently shared by the C and NASM backends. If a backend does not support a benchmark's language features, the suite must report that backend as unsupported rather than silently switching implementations.

## What is measured

- **Compile time** includes the YLang invocation, semantic checking, backend emission, and native assembly/compiler/linker run.
- **Run time** includes starting the executable and its exit; stdout/stderr capture and process creation remain part of the measurement. It is not a pure instruction-cycle benchmark.
- **Binary size** is the size of the final executable from the last measured build.
- Warm-up iterations are excluded from reported samples. P95 is the nearest-rank 95th percentile.

Each backend is measured separately. Do not compare results from different machines, compiler versions, optimization settings, power modes, or workloads as if they were controlled experiments. Record the commit SHA and all toolchain versions with published results.

## YLang 3.0 performance gate

The optimized LLVM backend must reach a median execution time no more than 5% slower than the equivalent optimized C baseline on each CPU-bound release workload, with no individual workload regression above 10%. The release review must also evaluate P95 latency, compile time, startup time, peak memory, and binary size.

This is an acceptance target, not a current result or a promise that LLVM will automatically meet it. The 3.0 release is blocked until the suite is expanded, measurements are reproducible, and the target is actually demonstrated. Safety checks must not be removed to win benchmarks.

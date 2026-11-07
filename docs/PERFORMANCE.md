# Reproducible Performance Measurements

**Status: development tooling. Numbers are machine-specific and should not be treated as release guarantees.**

## Run the native benchmark

Build YLang first, then run the same workload on the same host and toolchain before and after a change:

```sh
python3 benchmarks/bench-native.py benchmarks/native-loop.yl \
  --cc gcc --runs 30 --warmup 3 \
  --expected-file benchmarks/expected-native-loop.txt
```

Use `--cc clang` for the Clang path. The optional `--compiler /path/to/ylang` chooses the compiler executable. To compare a development compiler against the current stable tag on one host, run `python3 benchmarks/compare-release.py --baseline v2.2.0 --cc gcc --runs 30 --warmup 3`. Add `--keep-dir build/benchmark-artifacts` to preserve the final generated executable for inspection.

The report shows the YLang version, host platform and architecture, reported CPU model, selected native C compiler version, requested and effective optimization level, core C build flags, compile-time median and P95, execution-time median and P95, and executable byte size. The expected-output check ensures that the benchmark is still measuring a program with known behavior rather than a broken output path.

## What is measured

- **Compile time** includes the YLang invocation, semantic checking, C generation, and native compiler/linker run.
- **Run time** includes starting the executable and its exit; stdout/stderr capture and process creation are part of the measurement. This is not a pure instruction-cycle benchmark.
- **Binary size** is the size of the final executable produced by the last measured build.
- Warm-up iterations are not included in the reported samples. P95 is the nearest-rank 95th percentile.

The CI smoke test verifies that the harness executes. The release-validation workflow runs a 30-sample comparison against the latest stable compiler on the same Linux runner, using the same native compiler, optimization setting, and exact expected output. It measures compile latency and short-program launch cost with a compact loop, then measures runtime separately with a 10-million-iteration loop so process startup is a smaller part of the runtime result. It records the CPU model from the host when the Python platform API omits it, uploads both full reports and a ratio summary, and does not enforce a universal speed target. Review material compile-time and binary-size regressions even when execution time is unchanged. Do not compare measurements from different machines, native compiler versions, optimization settings, power modes, or workloads as if they were a controlled experiment.

## YLang 2.3 development sample

A 30-sample paired comparison against the 2.2.0 stable baseline was recorded on the GitHub-hosted Ubuntu runner with an AMD EPYC 7763 CPU. The full reports, including the exact compiler version and flags, are attached to [CI run #5079](https://github.com/Yassine-Jemi01/ylang/actions/runs/42371782216) for candidate commit `19038b8`.

| Metric | 2.3 candidate | 2.2.0 baseline | Difference |
|---|---:|---:|---:|
| Compile median | 81.339 ms | 77.324 ms | +5.2% (+4.015 ms) |
| Compile P95 | 82.621 ms | 78.685 ms | +5.0% |
| Short-program launch + run | 0.942 ms | 0.938 ms | +0.4% |
| 10-million-iteration run median | 5.717 ms | 5.710 ms | +0.1% |
| 10-million-iteration run P95 | 5.824 ms | 5.806 ms | +0.3% |
| Executable size | 22,840 bytes | 22,840 bytes | unchanged |

This sample shows a small compile-time overhead, while the measured runtime and executable size are effectively unchanged. It does **not** support a claim that YLang 2.3 is faster than 2.2. Treat these figures as one controlled CI comparison, not a guarantee for other machines; repeat on representative hardware before making performance claims.

## Suggested comparison record

For performance-sensitive changes, keep the commit SHA, YLang version, operating system/architecture, native compiler version, `YLANG_OPT_LEVEL`, run count, warm-up count, median, P95, binary size, and expected program output together. Re-run several times if a result is close to the noise floor. Prefer stable improvements across repeated measurements over a single faster number.

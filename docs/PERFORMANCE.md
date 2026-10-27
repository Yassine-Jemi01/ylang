# Reproducible Performance Measurements

**Status: development tooling. Numbers are machine-specific and should not be treated as release guarantees.**

## Run the native benchmark

Build YLang first, then run the same workload on the same host and toolchain before and after a change:

\`\`\`sh
python3 benchmarks/bench-native.py benchmarks/native-loop.yl \\
  --cc gcc --runs 30 --warmup 3 \\
  --expected-file benchmarks/expected-native-loop.txt
\`\`\`

Use \`--cc clang\` for the Clang path. The optional \`--compiler /path/to/ylang\` chooses the compiler executable. Add \`--keep-dir build/benchmark-artifacts\` to preserve the final generated executable for inspection.

The report shows the YLang version, host platform, selected native C compiler, compile-time median and P95, execution-time median and P95, and executable byte size. The expected-output check ensures that the benchmark is still measuring a program with known behavior rather than a broken output path.

## What is measured

- **Compile time** includes the YLang invocation, semantic checking, C generation, and native compiler/linker run.
- **Run time** includes starting the executable and its exit; stdout/stderr capture and process creation are part of the measurement. This is not a pure instruction-cycle benchmark.
- **Binary size** is the size of the final executable produced by the last measured build.
- Warm-up iterations are not included in the reported samples. P95 is the nearest-rank 95th percentile.

The CI smoke test verifies that the harness executes and reports its metrics; it intentionally does not enforce a universal speed target. Do not compare measurements from different machines, native compiler versions, optimization settings, power modes, or workloads as if they were a controlled experiment.

## Suggested comparison record

For performance-sensitive changes, keep the commit SHA, YLang version, operating system/architecture, native compiler version, \`YLANG_OPT_LEVEL\`, run count, warm-up count, median, P95, binary size, and expected program output together. Re-run several times if a result is close to the noise floor. Prefer stable improvements across repeated measurements over a single faster number.

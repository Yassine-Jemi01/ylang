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

## YLang 2.3.0 release measurement

The final release workflow recorded a 30-sample paired comparison of the exact
`v2.3.0` authorization commit `31e30605a08b068cf4c3a81256464c3a72666112`
against stable tag `v2.2.0`. Both compilers ran on the same GitHub-hosted
Linux x86-64 runner with GCC 15.2.0, `-O2`, and three warm-up runs. The host
reported an Intel Xeon Platinum 8573C.

| Measurement | YLang 2.3.0 | YLang 2.2.0 | Difference |
|---|---:|---:|---:|
| Compile median | 58.805 ms | 56.046 ms | +4.9% |
| Compile P95 | 61.056 ms | 61.134 ms | -0.1% |
| Launch + short run median | 0.566 ms | 0.555 ms | +2.0% |
| 10M-loop run median | 4.850 ms | 5.097 ms | -4.8% |
| 10M-loop run P95 | 5.351 ms | 5.457 ms | -1.9% |
| Compact executable size | 22,840 bytes | 22,840 bytes | unchanged |
| 10M-loop executable size | 22,848 bytes | 22,848 bytes | unchanged |

This final sample shows a modest compile-median regression, a slightly higher
short-program launch measurement, effectively unchanged binary size, and a
lower median for the long-loop workload. It is one controlled runner sample,
not evidence that YLang is generally faster or slower. The complete reports
and generated executables are attached to [stable publishing workflow #8](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464514).

## Earlier 2.3 development samples (not release measurements)

The two earlier 30-sample comparisons below were run against development
commits before the stable release authorization commit. Their compiler output
still reported version `2.2.0`, because the development branch had not yet
advanced `VERSION`; they must not be mistaken for measurements of the final
`v2.3.0` tag. They are retained only as development history.


Two 30-sample paired comparisons against the 2.2.0 stable baseline were recorded on GitHub-hosted Ubuntu runners. The full reports, including exact compiler versions and flags, are attached to [CI run #5079](https://github.com/Yassine-Jemi01/ylang/actions/runs/42371782216) and [CI run #5088](https://github.com/Yassine-Jemi01/ylang/actions/runs/42372281463).

| Sample | CPU | Compile median (candidate / baseline) | 10M loop median (candidate / baseline) | Executable size |
|---|---|---:|---:|---:|
| #5079 | AMD EPYC 7763 | 81.339 / 77.324 ms (+5.2%) | 5.717 / 5.710 ms (+0.1%) | 22,840 / 22,840 bytes |
| #5088 | AMD EPYC 9V74 | 65.586 / 61.902 ms (+6.0%) | 4.946 / 4.937 ms (+0.2%) | 22,840 / 22,840 bytes |

Short-program launch plus execution was +0.4% in #5079 and +3.8% in #5088. The measured long-loop runtime and executable size are effectively unchanged; compile median is consistently about 5–6% higher (roughly 3.7–4.0 ms in these samples).

Compile P95 was stable in #5079 but varied sharply in #5088, so P95 compile latency is not yet a reliable claim from these shared runners. The evidence supports neither a general speed claim nor a material runtime/binary-size regression. Repeat on representative hardware before making performance claims or treating the P95 result as a release guarantee.

## Suggested comparison record

For performance-sensitive changes, keep the commit SHA, YLang version, operating system/architecture, native compiler version, `YLANG_OPT_LEVEL`, run count, warm-up count, median, P95, binary size, and expected program output together. Re-run several times if a result is close to the noise floor. Prefer stable improvements across repeated measurements over a single faster number.

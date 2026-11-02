# Native backend benchmarks

Run the benchmark harness after building YLang:

```sh
python3 benchmarks/bench-native.py benchmarks/native-loop.yl \
  --runs 10 --warmup 2 \
  --expected-file benchmarks/expected-native-loop.txt
```

The harness records YLang compile latency, executable launch plus runtime latency,
binary size, host information, the native compiler version, and the exact compiler
argument vector. It checks the program's output on every warm-up and measured run.

## Workload matrix

| Workload | Main coverage | Expected output |
|---|---|---|
| `native-loop.yl` | Integer loop and accumulation | `expected-native-loop.txt` |
| `native-branches.yl` | Branches, modulo, and loop-carried updates | `expected-native-branches.txt` |
| `native-calls.yl` | Direct function calls and integer arithmetic | `expected-native-calls.txt` |
| `native-f64.yl` | f64 conversion, arithmetic, comparison, and calls | `expected-native-f64.txt` |

Run all workloads through the regression smoke test:

```sh
python3 tests/benchmark-smoke.py
```

Measurements are descriptive, not universal pass/fail performance targets. Compare
runs only on the same machine, OS, YLang revision, native compiler, flags, optimization
level, and workload. The smoke test validates correctness and report completeness;
it deliberately does not assert timing thresholds.

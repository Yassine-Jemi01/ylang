# YLang 3.0: Unified Product and Engineering Roadmap

**Status:** Active development  
**Development baseline:** YLang 2.2.0  
**Next release:** YLang 3.0.0  
**Policy:** The previously proposed 2.3 feature line is folded into 3.0. No separate 2.3 stable release is planned.

YLang 3.0 is a single major effort: modernize the language, complete the native backends, establish an unmistakable reason to choose YLang, and prove performance with reproducible measurements.

## 1. Product purpose: why YLang?

**YLang is a practical native language for building fast developer tools, desktop utilities, and 2D games without making the developer fight the toolchain.**

The intended combination is:

- Native binaries and predictable performance.
- Rust-influenced clarity, without copying Rust's syntax wholesale.
- Explicit mutability and a progressively stronger ownership model.
- Helpful error handling with both `Result` and exceptions.
- A batteries-included standard library and first-party tooling.
- One coherent compiler workflow across supported platforms.
- First-party GUI and 2D APIs for small applications and games.

The language should feel approachable to a developer coming from C, Python, or Go, while still exposing the control needed for native software.

This is the 3.0 product goal, not a claim that every library or platform listed here is complete today.

## 2. Language identity and syntax

### 2.1 Bindings: immutable by default

Preferred syntax:

```ylang
let name: string = "YLang";
let count: int = 10;
let mut score: int = 0;

score += count;
```

- `let name: Type` creates an immutable binding.
- `let mut name: Type` creates a mutable binding.
- The old `let Type name` syntax remains accepted during migration with warning `W2301`.
- Legacy declarations must retain their old mutability semantics; they must never silently change meaning.

The compiler enforces immutable bindings for locals, globals, function parameters, array elements, and struct field paths. Immutable fixed-size arrays can be initialized and read; attempts to mutate their elements or fields through an immutable root are rejected with E2022. Mutable arrays remain element-mutable.

### 2.2 Function declarations

Use `fn` with YLang's typed parameters and arrow return type. The old `function` spelling remains accepted during the 3.0 migration period with deprecation warning `W2302`; new code uses `fn`.

```ylang
fn distance(int x, int y) -> float {
    return math.sqrt(
        to_float(x * x + y * y)
    );
}
```

The language should gain useful Rust-inspired features while keeping its own visual identity.

### 2.3 Error handling

Both mechanisms are part of the 3.0 design:

- `Result<T, E>`, `Ok`, `Err`, exhaustive matching, and `?` for expected/recoverable errors.
- `throw` and `try/catch` for exceptional failures and interoperability with throwing runtime operations.

The two mechanisms must remain explicit and separate. Returning `Err` must not silently throw an exception.

### 2.4 Features that improve everyday programming

Prioritize these after the binding and error foundations:

- Payload enums and exhaustive pattern matching.
- General generics and type constraints.
- `defer` for deterministic cleanup: evaluate scalar arguments immediately, run direct `fn` calls in LIFO order when the current lexical scope exits, including `return`, `break`, and `continue`. The first implementation supports scalar/enum captures and C code generation; string/aggregate captures and NASM parity remain incomplete until ownership and cleanup behavior is implemented and tested. It rejects built-ins, non-void cleanup functions, array parameters, and string/aggregate captures with dedicated diagnostics.
- Slices and safe dynamic collections.
- Iterators and ergonomic `for item in collection` loops.
- Limited type inference where it improves readability without hiding public API types.
- String interpolation and Unicode-aware text operations.
- Explicit module visibility and a reproducible project system.
- A documented safe/unsafe FFI boundary.

Do not introduce syntax only for marketing. Every new construct needs a clear semantic rule, compiler diagnostics, runnable examples, and tests.

## 3. Memory model and safety

YLang should not require a tracing garbage collector for its core memory model.

The 3.0 direction is deterministic ownership and borrowing:

- One clear owner for owned allocations.
- Explicit moves and well-defined borrowing.
- Rejection of use-after-move and dangling borrows.
- Scope cleanup on normal exit, return, break, continue, exceptions, and Result propagation.
- Checked bounds and arithmetic by default.
- Audited unsafe FFI boundaries rather than silently unsafe behavior.

Do not claim Rust-level memory safety until the borrow checker and runtime contracts are complete and tested. Do not add raw pointers as ordinary safe values.

## 4. Native backends

### 4.1 LLVM is the defining 3.0 backend

Required architecture:

```text
Source
  -> Lexer / Parser
  -> Module resolution / Type checking
  -> Typed YLang IR
  -> IR verifier
  -> LLVM IR
  -> LLVM verification / Optimization
  -> Native object
  -> Linker + YLang runtime
  -> Executable
```

The C backend remains a behavioral reference and compatibility option while LLVM is developed.

LLVM completion means more than generating a hello-world module. It must support the stable 3.0 language contract, emit native objects, link the runtime, preserve debug information, and pass differential tests.

See [RFC-3.0-LLVM-BACKEND.md](RFC-3.0-LLVM-BACKEND.md).

### 4.2 Complete the NASM backend

The NASM backend is a first-class native target for Linux x86-64, not a cross-platform backend. The 3.0 branch has already expanded its baseline beyond the original demo subset.

Implemented on the development branch:
- Short-circuit `and` / `or`, preserving left-to-right side effects.
- Scalar global storage and reads/writes for integer, boolean, character, enum, and literal-initialized string values.
- System V AMD64 integer-class argument passing: the first six arguments use registers and additional scalar arguments use the caller's stack area with 16-byte call-site alignment.
- Differential coverage for globals, short-circuit evaluation, and nested calls with eight arguments.

The current branch now also supports fixed-size local/global arrays, indexed reads and writes, negative/upper-bound traps, and pointer-plus-length array parameters, with C/NASM parity coverage. Remaining limitations include floating-point values and formatting, f-strings, string comparisons, structs, owned strings, payload enums, broad standard-library calls, and GUI/network APIs.

Next priorities:
1. Floating-point arithmetic, argument passing, and formatting using the x86-64 SSE2 ABI.
2. String comparisons, formatting, and UTF-8-safe runtime helpers.
3. Struct values, payload enums, and stable module/import parity.
4. More standard-library operations with explicit runtime contracts.
5. Ownership-aware cleanup and error propagation after runtime semantics are defined.
6. Complete the test-enforced capability matrix and precise rejection diagnostics for constructs outside the NASM subset.

Every expanded feature requires C/NASM differential tests for standard output, error behavior, and exit status. Do not mark a feature supported because assembly emission alone succeeds; the assembled executable must run correctly.

### 4.3 Backend capability matrix

Maintain a test-enforced capability matrix for C, NASM, and LLVM. Frontend acceptance must not be confused with backend support. Unsupported constructs must fail before emission; no silent fallback is allowed.

## 5. Standard library: batteries included

The goal is to make common application development pleasant without sacrificing native control.

Priorities:

- **Text:** UTF-8 strings, Unicode-safe iteration, parsing, formatting.
- **Collections:** vectors, slices, maps, sets, iterators.
- **Filesystem:** safe file handles, paths, directories, metadata, and binary I/O.
- **Serialization:** built-in JSON parsing and serialization.
- **Networking:** HTTP client, sockets, timeouts, TLS through explicit platform/runtime dependencies.
- **Concurrency:** threads, synchronization, atomics, and a documented memory model.
- **Async:** general async functions, scheduler behavior, cancellation, and structured concurrency. Existing async support is currently limited to HTTP task handles.
- **Applications and games:** stable GUI and 2D drawing/input APIs with documented optional dependencies.

Each API should return typed errors where failures are expected. Avoid APIs that hide failure in global mutable state.

## 6. Developer experience and ecosystem

A language that is fast but unpleasant to use will not earn adoption.

3.0 should include:

- Official formatter with stable formatting rules.
- Full Language Server Protocol support: completion, diagnostics, go-to-definition, references, rename, and symbol navigation.
- Source-level debugger with breakpoints, scopes, locals, and call stacks.
- Project commands such as `ylang new`, `build`, `run`, `test`, `fmt`, and `doc`.
- Project manifest, dependency resolution, lockfile, and local cache.
- Official package registry or a clearly defined distribution model.
- Versioned standard-library documentation and tutorials.
- Useful compiler diagnostics and safe automated fixes.
- VS Code, Neovim, and Zed support with feature parity described honestly.

YSPM remains the Linux system package manager integration. It is not a substitute for project-local dependency resolution.

### 6.1 Official Windows support

Windows x86-64 is a first-class YLang 3.0 target, not an experimental port.

The initial officially supported Windows toolchain is **MinGW-w64 GCC through MSYS2 MINGW64**. MSVC is not part of the 3.0 support promise unless it receives its own compiler, runtime, linker, dependency, and CI validation. A clearly documented supported toolchain is preferable to claiming untested toolchain parity.

Windows 3.0 acceptance requires:

- Clean compiler builds from a fresh checkout and a documented native installation path.
- The full applicable compiler and integration suite on Windows, not only a hello-world smoke test.
- Native executable build, linking, startup, process exit codes, standard input/output/error, filesystem operations, environment variables, and command-line paths with spaces.
- Relative and search-path imports, case-insensitive path identity, Windows path-list separators, and diagnostics with correct source paths.
- Stable HTTP/libcurl and SDL2 GUI/Game2D build/runtime paths with optional dependency detection and feature-specific tests.
- The stable CLI debugger on Windows through the documented MinGW-w64 GDB toolchain.
- LLVM native object generation and executable linking for the supported Windows target triple, with C/LLVM parity tests.
- Windows-specific ownership, cleanup, error handling, Unicode/path behavior, packaging, and upgrade tests.
- CI on a clean Windows runner for every release candidate; a focused smoke workflow alone is insufficient.

The support promise is for the documented Windows x86-64 MinGW-w64 configuration. Do not imply MSVC, Windows ARM64, NASM-on-Windows, or an untested combination is supported.

### 6.2 Graduate every existing experimental subsystem

YLang 3.0 has a firm product rule: **none of the features already shipped or documented as experimental may remain experimental in the 3.0 stable release**. Each must either meet its stable acceptance criteria below or be removed from the advertised 3.0 feature set before release. Renaming a heading or deleting the word "experimental" does not count as implementation.

| Current experimental subsystem | Required 3.0 stable contract |
|---|---|
| Native Windows | Official Windows x86-64 MinGW-w64 support meeting section 6.1, with full CI and installation documentation. |
| Named imports and multi-file modules | Stable module resolution, deterministic duplicate/cycle diagnostics, relative and configured search paths, reproducible builds, and C/LLVM parity. |
| Structs | Stable construction, field access, assignment, parameter passing, return values, nested values, and defined layout/ownership rules; unsupported methods, inheritance, or recursive layouts must be explicitly outside the 3.0 language contract. |
| Enums and matching | Stable enum identity, payload variants, exhaustive matching, payload binding, and defined behavior across functions, modules, and both stable backends. |
| GUI and Game2D | Stable documented SDL2 APIs for window lifecycle, events/input, timing, drawing, error reporting, and cleanup on Linux, macOS, and Windows. This is a small 2D application/game API, not a promise of a complete commercial game engine. |
| NASM backend | A supported, documented Linux x86-64 backend with an enforced capability matrix, complete tests for every advertised construct, safe rejection outside its subset, and C/NASM parity for the shared subset. Linux-only scope is intentional, not experimental status. |
| HTTP and HTTP-task async | Stable synchronous request and HTTP-task APIs, typed/consistent failure behavior, cancellation/consumption rules, TLS/redirect/timeouts/size limits, loopback integration tests, and optional libcurl setup on every supported desktop platform. General-purpose async functions remain a separate language feature, not an implied promise of this API. |
| Owned and borrowed strings / memory model | Complete ownership transfer, borrowing/lifetime rules, cleanup for every supported storage form and control-flow exit, and sanitizer/leak tests. Do not advertise Rust-level memory safety unless the implemented model justifies that exact claim. |
| CLI debugger | Stable source-aware debug builds and interactive debugger launch on Linux, macOS, and Windows for the supported C/LLVM paths, with documented breakpoint, stack, and local-variable behavior. A dedicated VS Code Debug Adapter Protocol UI is a separate deliverable unless explicitly completed. |
| Nix Flake | Reproducible, tested Nix package and development shell for the declared Linux architecture, with pinned inputs and clean-build CI. Nix's own opt-in Flake configuration requirement is not a YLang feature limitation. |
| YSPM integration | Reproducible Linux x86-64 package creation, metadata/ABI validation, installation, upgrade/removal, and exact-artifact release CI. It remains a Linux system-package format, not a Windows or macOS installer. |
| LLVM backend | Complete stable 3.0 language/runtime coverage on each advertised target, native object emission, linking, debug metadata, optimization, and differential parity. The current scalar Clang-mediated prototype is not the stable endpoint. |

A graduated subsystem may still have a deliberately narrow, documented scope. Stable means its published contract is implemented, tested, diagnosable, versioned, and supported within that scope; it does not mean every backend supports every construct or that optional dependencies are bundled.

Release-wide graduation gates:

1. Every row has a feature owner, a stable specification, runnable examples, negative tests, and a test-enforced capability matrix where applicable.
2. The supported feature set passes Linux GCC/Clang, macOS Clang, and Windows MinGW-w64 CI. Optional GUI/network features also run their platform dependency tests.
3. C and LLVM agree on observable behavior for their shared supported language set. NASM agrees on its explicitly advertised subset.
4. No feature marked experimental is exposed as stable merely because the parser accepts its syntax.
5. README, language specification, Windows/Nix/YSPM/HTTP/debugger/GUI/backend guides, online book, website, editor metadata, release notes, and package metadata all describe the same stable support matrix.
6. A repository audit before the release candidate finds no current subsystem still described as experimental. The stable release notes must state any intentionally unsupported toolchain/platform explicitly.


## 7. Performance: a release gate, not a slogan

YLang 3.0 must be fast because of measured compiler and runtime behavior, not because the documentation says so.

### 7.1 Benchmarks

Maintain representative workloads for:

1. Integer and floating-point arithmetic.
2. Branching and loops.
3. Function calls and recursion.
4. Arrays, structs, and memory access.
5. Strings, allocations, and cleanup.
6. File and network I/O.
7. Program startup and executable size.
8. Compiler build time and incremental rebuild time.

### 7.2 Comparison rules

- Compare the same workload, inputs, safety checks, and output validation.
- Record CPU, OS, compiler version, LLVM version, optimization level, and commit.
- Use warmups, repeated runs, medians, and P95.
- Keep compile time, execution time, and binary size as separate metrics.
- Compare C, NASM, and LLVM where they support the same semantics.
- Include equivalent C and Rust baselines for public performance reports when practical.
- Never remove overflow or bounds checks merely to win a benchmark.

### 7.3 Performance goals

The intended goal for optimized LLVM builds is to stay within 5% of the equivalent optimized C baseline on the CPU-bound benchmark suite, with no major workload regressions. This is a target to validate, not a current result or guarantee.

The release review must also examine binary size, startup time, compile time, memory use, and debug-build performance. No single benchmark is sufficient.

The 3.0 release must not be declared complete until the optimized backend demonstrates stable, reproducible performance across the workload suite.

## 8. Implementation milestones

### M0 — 3.0 foundation
- Adopt 3.0 as the single next release target.
- Establish the language purpose and design contract.
- Introduce new immutable/mutable binding syntax.
- Preserve legacy syntax with deprecation warnings.
- Add lexer/parser, semantic, and editor tests.

### M1 — Type system and errors
- Complete Result and Option.
- Add payload enums and exhaustive matching.
- Implement try/catch and throw.
- Integrate all error exits with ownership cleanup.
- Add generic type instances and then general generics.

### M2 — Memory model
- Complete borrowing/lifetime checking.
- Extend ownership to arrays, structs, and collections.
- Test exceptional and early exits under sanitizers.

### M3 — NASM expansion
- [x] Add scalar globals and short-circuit boolean logic.
- [x] Implement System V AMD64 stack-passed scalar arguments and 16-byte call alignment.
- [x] Add C/NASM parity coverage for globals, short-circuit side effects, and eight-argument nested calls.
- [x] Add fixed-size local/global arrays, indexed reads/writes, negative and upper bounds checks, and array-parameter pointer/length ABI lowering with C/NASM parity tests.
- [ ] Extend NASM arrays to floating-point and ownership-bearing element types; arrays still cannot be returned or nested.
- [ ] Add floating point, strings, and standard-library lowering.
- [ ] Publish a test-enforced capability matrix.

### M4 — Backend-neutral typed IR
- [x] Define the first backend-neutral typed SSA IR core: scalar types, values, functions, blocks, arithmetic/comparison instructions, and explicit terminators.
- [x] Implement an independent verifier for SSA definitions, typed operands/results, returns, and branch targets.
- [x] Add PHI nodes, predecessor validation, and dominator-based cross-block SSA verification.
- [x] Add typed function calls and void calls.
- [x] Lower scalar numeric/bool functions, locals, arithmetic, short-circuit logic, if/while/for control flow, PHI merges, and scalar print.
- [ ] Add globals, arrays, strings, aggregate memory operations, and complete source metadata.
- [ ] Extend ownership, errors, enums, imports, and standard-library lowering.
- [ ] Keep semantics out of backend-specific code.

### M5 — LLVM executable prototype
- [x] Emit verified textual LLVM IR for the initial scalar subset.
- [x] Add explicit `emit-llvm` and experimental Linux x86-64 `build-llvm` commands using Clang.
- [x] Lower checked integer overflow and division failures to the YLang runtime failure path.
- [x] Add a Linux x86-64 scalar `emit-obj` prototype using Clang to compile textual LLVM IR into a relocatable object; link and run it in differential tests.
- [ ] Select and isolate a stable LLVM API boundary and target-machine implementation.
- [ ] Replace the Clang-mediated object prototype with LLVM target-machine emission and link the complete YLang runtime.
- [ ] Add debug metadata and target detection.

### M6 — LLVM language coverage
- Lower all stable language features.
- Integrate runtime, ownership, exceptions, Result, imports, and standard library.
- Add backend differential tests.

### M7 — Optimization support (without the final performance gate)
- [ ] Implement and test `-O0`, `-O1`, `-O2`, `-O3`, and size optimization on the stable backend feature set.
- [ ] Verify that optimization levels preserve overflow checks, bounds checks, ownership cleanup, error behavior, and observable evaluation order.
- [ ] Keep the benchmark workloads and measurement scripts ready, but do not run the final comparative performance campaign yet.

### M8 — Windows and experimental-feature graduation
- [ ] Promote Windows x86-64 MinGW-w64 from smoke-tested support to a fully tested, officially supported target.
- [ ] Complete Windows runtime, imports, HTTP, SDL2, debugger, filesystem, process, path, ownership, and native-linking coverage.
- [ ] Graduate named imports, structs, enums, GUI/Game2D, NASM, HTTP/HTTP-task async, owned/borrowed strings, the CLI debugger, Nix packaging, and YSPM packaging under section 6.2.
- [ ] Add payload enums/matching and complete ownership cleanup before declaring those subsystems stable.
- [ ] Publish and enforce the C/NASM/LLVM/platform feature capability matrix.
- [ ] Remove obsolete experimental labels only after each feature passes its stable acceptance gate.

### M9 — Release hardening
- [ ] Verify all supported targets in clean native CI, including Windows x86-64 MinGW-w64.
- [ ] Complete documentation, packages, formatter, LSP, debugger, install/upgrade, and migration workflows.
- [ ] Run sanitizers, fuzzing, C/NASM/LLVM parity, GUI/network integration, and reproducibility tests.
- [ ] Audit the README, website, online book, editor metadata, package metadata, and all feature guides: no existing experimental subsystem may remain experimental in the 3.0 release contract.
- [ ] Confirm that the exact release candidate passes the full suite before beginning performance verification.

### M10 — Final performance gate (owner-run)
- [ ] Begin only after M0-M9 are complete and the exact candidate passes all correctness, platform, packaging, and documentation gates.
- [ ] Hand the benchmark suite and methodology to the project owner for final execution and review.
- [ ] Do not start further performance-driven code changes or publish performance claims until the owner has inspected and accepted the measurements.
- [ ] Record CPU, OS, compiler/LLVM versions, optimization levels, commit, warmups, repeated samples, medians, P95, binary size, startup, and compile time.
- [ ] Release 3.0 only after the owner accepts the reproducible results and every other release gate passes.

## 9. Release policy

- No separate 2.3 stable release is planned; its language work is absorbed into 3.0.
- Keep C as the reference backend until LLVM reaches parity.
- Make Windows x86-64 with MinGW-w64 an officially supported, fully CI-tested target; do not claim MSVC or other untested Windows targets.
- Graduate every subsystem currently marked experimental under section 6.2 before the 3.0 stable release.
- Keep NASM Linux x86-64-specific with a stable, explicit, test-enforced capability matrix.
- Keep optional platform dependencies explicit; optional does not mean experimental.
- Do not silently change language semantics between backends.
- Do not claim memory safety before the model is complete.
- Do not claim speed before reproducible measurements exist.
- Do not publish 3.0 while any required stable feature, supported platform, or current experimental subsystem lacks its acceptance evidence.

## 10. Definition of success

A developer should choose YLang because it lets them build native tools, desktop utilities, and 2D games with:

- Readable syntax and explicit control.
- Fast native output.
- Predictable memory and error behavior.
- A useful standard library.
- Integrated project, formatting, debugging, and package tools.
- A single coherent experience across the platforms YLang actually tests.

The goal is a language developers enjoy using and can trust—not a syntax clone or a collection of unfinished compiler backends.

**End of YLang 3.0 roadmap.**

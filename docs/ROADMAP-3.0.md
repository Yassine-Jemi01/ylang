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

Remaining limitations include floating-point values and formatting, arrays and bounds checks, f-strings, string comparisons, structs, owned strings, general standard-library calls, and GUI/network APIs.

Next priorities:
1. Fixed-size arrays, array indexing, array parameters, and bounds-check parity.
2. Floating-point arithmetic and formatting using the x86-64 SSE2 ABI.
3. String comparisons, formatting, and UTF-8-safe runtime helpers.
4. More standard-library operations with explicit runtime contracts.
5. Struct values, payload enums, and module/import parity.
6. Ownership-aware cleanup and error propagation only after the runtime semantics are defined.
7. Precise unsupported-feature diagnostics for anything that remains outside the backend subset.

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
- Define IR types, values, blocks, instructions, terminators, and source metadata.
- Implement lowering from the resolved frontend.
- Build an independent IR verifier.
- Keep semantics out of backend-specific code.

### M5 — LLVM executable prototype
- Select and isolate an LLVM API boundary.
- Generate verified LLVM IR.
- Emit native objects and link a minimal executable.
- Add debug metadata and target detection.

### M6 — LLVM language coverage
- Lower all stable language features.
- Integrate runtime, ownership, exceptions, Result, imports, and standard library.
- Add backend differential tests.

### M7 — Optimization and performance
- Implement and test `-O0`, `-O1`, `-O2`, `-O3`, and size optimization.
- Run the benchmark suite and profile hot paths.
- Improve compile time, runtime, startup, and binary size based on evidence.

### M8 — Release hardening
- Verify all supported targets in native CI.
- Complete documentation, package, formatter, LSP, and debugger workflows.
- Run sanitizers, fuzzing, parity, and reproducibility tests.
- Publish benchmark methodology and results.
- Release 3.0 only after all release gates pass.

## 9. Release policy

- No separate 2.3 stable release is planned; its language work is absorbed into 3.0.
- Keep C as the reference backend until LLVM reaches parity.
- Keep NASM explicitly Linux x86-64 and capability-gated.
- Do not silently change language semantics between backends.
- Do not claim memory safety before the model is complete.
- Do not claim speed before reproducible measurements exist.
- Do not publish 3.0 while any required stable language feature lacks a correct backend implementation.

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

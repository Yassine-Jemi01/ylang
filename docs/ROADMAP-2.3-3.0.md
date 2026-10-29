# YLang 2.3 → 3.0: Product and Engineering Roadmap

**Status:** Development plan  
**Current repository baseline:** YLang 2.2.0 stable  
**Next feature line:** 2.3.x  
**Next major release target:** 3.0.0 with a complete LLVM backend

This roadmap separates language maturity from backend completion. YLang 2.3 improves the language contract and error model. YLang 3.0 is gated on a complete, tested LLVM backend and a coherent native toolchain.

## 1. What makes a language excellent?

A great language is not defined by the number of keywords it has. It needs a consistent type system, predictable runtime semantics, excellent diagnostics, useful libraries, reliable tools, portability, and a clear compatibility policy.

YLang should prioritize the following gaps.

## 2. Priority gaps

### P0 — Compiler and language correctness

- Make mutability explicit and enforce it for variables, arrays, and struct fields.
- Finish Result, Option, exceptions, and exhaustive payload matching with one coherent error model.
- Define ownership and borrowing rules for strings, arrays, structs, and error payloads.
- Guarantee cleanup across return, break, continue, exceptions, and Result propagation.
- Keep evaluation order, overflow checks, bounds checks, and runtime behavior specified.
- Maintain a capability matrix for C, NASM, and LLVM; unsupported syntax must never silently change behavior.

### P1 — Type-system maturity

- General generics and type constraints after built-in Result/Option semantics are proven.
- Limited type inference where it improves ergonomics without making APIs ambiguous.
- Payload enums and exhaustive pattern matching.
- Slices and dynamic collections with documented ownership and bounds behavior.
- A documented FFI boundary with explicit safe/unsafe rules.

Do not add an inheritance-based OOP system merely for feature count. Structs, enums, composition, and later interfaces/traits may provide a more coherent model.

### P2 — Standard library and runtime

- Unicode-aware strings and character iteration. The current string/char model is byte-oriented and should not be marketed as full Unicode text handling.
- Dynamic vectors, slices, maps, iterators, and a consistent collection API.
- Built-in JSON parsing/serialization and robust text/binary I/O.
- A stable filesystem, path, process, environment, and time API.
- General-purpose async functions, scheduler semantics, cancellation, and structured concurrency. Current async support is limited to HTTP task handles.
- Stable platform abstractions and optional GUI/network feature detection.

### P3 — Developer experience

- A formatter with stable formatting rules.
- A maintained Language Server Protocol implementation.
- Complete debugger integration with breakpoints, scopes, locals, and call stacks.
- Better type diagnostics, warnings, suggestions, and automated safe fixes.
- A project build tool with reproducible manifests, dependency resolution, lockfiles, and local caching.
- Keep YSPM's role distinct: YSPM is the Linux system package manager integration, not a project dependency resolver for `.yl` modules.
- A complete standard library reference, practical book, runnable examples, and migration guides.

### P4 — Performance, portability, and trust

- Complete LLVM backend and native object emission.
- Reproducible compiler/runtime benchmarks and binary-size measurements.
- Cross-target builds with explicit target triples, runtime libraries, and tested linkers.
- Fuzzing and sanitizer coverage for the frontend, IR, runtime, and backends.
- Security review for FFI, file/process APIs, networking, and memory ownership.
- Clear documented support tiers for Linux, macOS, and Windows based on actual CI.

## 3. YLang 2.3 scope

2.3 should deliver a coherent language improvement rather than attempting to solve every ecosystem gap at once.

### 2.3 language work
1. New immutable binding: `let name: Type = value;`
2. New mutable binding: `let mut name: Type = value;`
3. Legacy declarations remain accepted with deprecation warnings.
4. `try/catch` and `throw` with well-defined propagation and cleanup.
5. Built-in `Result<T, E>`, `Ok`, `Err`, exhaustive matching, and `?`.
6. Semantic checks for all mutation paths.
7. Runtime error handling and stable diagnostics.

### 2.3 engineering work
- Update AST and mutability representation.
- Keep C as the reference backend.
- Add control-flow and cleanup tests before adding more syntax.
- Update Tree-sitter, VS Code, Neovim, Zed, and the online book.
- Keep LLVM development behind an explicit backend option until it passes its own gates.

The detailed language design is in [RFC-2.3-001](RFC-2.3-001.md).

## 4. LLVM and YLang 3.0

A full LLVM backend is the defining goal for 3.0.

It requires:
1. Backend-neutral typed IR.
2. IR verifier and lowering tests.
3. LLVM API integration isolated from the C frontend.
4. LLVM IR generation and verification.
5. Native object emission and executable linking.
6. Shared runtime ABI.
7. Support for all stable 3.0 language features.
8. Debug information and optimization levels.
9. Differential C/LLVM testing.
10. Platform-specific CI and packaging.

A prototype that compiles a trivial program is not a complete backend. The complete requirements are in [RFC-3.0-001](RFC-3.0-LLVM-BACKEND.md).

## 5. Proposed release sequence

| Stage | Focus | Exit condition |
|---|---|---|
| 2.3.0 | Bindings, exceptions, Result | Syntax and semantics pass frontend/runtime tests |
| 2.3.x | Stabilization and diagnostics | No known cleanup or error-path regressions |
| 2.4.x (optional) | Generics, collections, Unicode foundations | Type and runtime contracts are stable |
| 3.0.0 | LLVM backend and native toolchain | All LLVM release gates pass |

The 2.4 line is optional and must not become a reason to weaken the 3.0 LLVM gates. Features may be moved between minor releases based on implementation evidence.

## 6. Release policy

- Keep documented stable behavior separate from experimental extensions.
- Require tests and documentation in the same change as new semantics.
- Do not claim memory safety until the ownership/borrowing model is complete and audited.
- Do not claim cross-platform support without native CI and runtime tests.
- Do not claim LLVM completion from IR emission alone.
- Do not make LLVM the default until the 3.0 release gates pass.
- Preserve the C backend as a reference and compatibility option where practical.

## 7. Definition of "legendary"

YLang will be in a strong position when a developer can:

- Install it easily on supported systems.
- Build a native project without depending on a C compiler in LLVM mode.
- Understand compiler errors without reading compiler source.
- Use safe collections, strings, errors, and modules without fighting the language.
- Debug original YLang source.
- Reuse packages with reproducible builds.
- Call native libraries through a documented FFI boundary.
- Trust that the same program behaves consistently across supported backends and targets.

That is the target: a coherent language and toolchain, not a checklist of fashionable syntax.

**End of roadmap.**

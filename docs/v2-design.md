# YLang 2.0 — Product and Language Design

**Status:** Development blueprint, not a claim that these features already exist. The stable 1.0.0 contract on `main` remains unchanged until a reviewed 2.0 release.

## Product goal

YLang 2.0 should be a practical, compiled general-purpose language for command-line tools, desktop utilities, services, and larger multi-file applications. Its identity is: **clear code, useful compiler feedback, predictable ownership, and one consistent toolchain on Linux and Windows**.

The language should feel approachable without hiding costs or weakening type checking. Avoid features that only add novelty or require users to memorize many special cases.

## Design principles

1. **Safe defaults.** No arbitrary pointer arithmetic in safe code; array indexing is checked; initialization, types, ownership, and borrowing are checked before native code generation.
2. **Visible costs.** Copying heap values is explicit; moving is cheap; allocation and I/O failures have documented behavior.
3. **Simple surface, strict core.** A small number of orthogonal constructs should compose instead of adding a keyword for every use case.
4. **Actionable diagnostics.** Errors show the location, stable code, reason, and a concrete correction when the compiler can be confident.
5. **Native compilation.** The target architecture is a dedicated YLang compiler pipeline and native backend, not making generated C the permanent language implementation. The current C backend remains a bootstrap and compatibility path until the new backend is tested.
6. **Portable by contract.** Language semantics must not depend on Linux-specific APIs. Windows is a required CI target, not a final afterthought.
7. **Open-source dependencies with provenance.** Prefer maintained, permissively licensed libraries with clear security practices. Pin versions, record licenses and update policy, and test the exact versions used. Do not add a dependency merely because it is popular.
8. **No unsupported safety claims.** Rust-level guarantees require a complete checker, sound compiler transformations, and adversarial testing—not syntax that resembles Rust.

## Language core

### Types

The intended built-in types are:
- Scalars: `int` (signed 64-bit), `float` (64-bit), `bool`, and `char`.
- `string`: UTF-8 text with explicit length; indexing semantics must be specified as bytes or Unicode scalar values before shipping.
- `T[]`: homogeneous, dynamically sized one-dimensional arrays.
- Function types, unit/void, and a small set of generic container/result types only after type inference and diagnostics are designed.

No implicit narrowing conversions. Conversion functions must make failure or loss of precision visible.

### Ownership and borrowing

- Scalars are copied.
- Heap-backed strings and arrays have one owner by default and move when ownership is transferred.
- Copying heap-backed values requires an explicit clone operation.
- `&T` grants shared read access; `&mut T` grants exclusive mutable access.
- First ship call-scoped borrows. Do not permit references to be returned or stored until lifetime analysis is implemented and tested.
- The compiler must reject use-after-move, conflicting mutable/shared borrows, and mutation through shared borrows.
- Every owned value is released exactly once on scope exit, return, loop exits, and failure paths.
- No user-visible unsafe pointer API in the first 2.0 release.

### Arrays and iteration

- One-dimensional homogeneous arrays with zero-based indexing.
- `len(array)` is safe and constant-time.
- Index reads/writes perform bounds checks unless a proof safely removes the check.
- Append has explicit ownership/capacity semantics and must handle allocation failure.
- Empty arrays and nested arrays must have unambiguous element types; nested arrays may be deferred if they threaten release quality.
- Range iteration must not overflow silently at integer boundaries.

### Functions, modules, and errors

- Keep explicit function parameter and return types for the initial 2.0 release.
- Add multi-file modules with explicit imports and visibility; define path resolution, duplicate exports, and cycle behavior before implementation.
- Prefer typed return values for recoverable failures; reserve process termination for entry-point/runtime-fatal failures. A generic `Result<T, E>` is a candidate only after generics are implemented soundly.
- Do not ship exceptions, async, macros, classes, or unrestricted generics merely to match other languages. Each needs a separate design and motivating use cases.

- `for (T item in values)` iterates over a snapshot of the array length for `int[]`, `float[]`, `bool[]`, and `char[]`. `string[]` iteration is deferred until element borrowing is designed. Numeric range loops remain planned.

## Standard library scope

**First implemented built-ins on `dev/lsp-foundation`:** `read_line()` reads one line from standard input, removes LF and an optional preceding CR; `len(text)` returns UTF-8 byte length; `clone(text)` creates an independent heap copy; and `parse_int(text)` / `parse_float(text)` validate complete numeric strings, rejecting trailing junk, overflow, and non-finite floats. Named string values move on initialization, assignment, by-value function calls, and returns. The checker rejects use after move and conservatively merges move states across branches/loops. Heap strings are still tracked until process exit, so scope-based deterministic cleanup remains unfinished. End-of-input before any characters is currently a runtime error.



The initial library should cover:
- `std.io`: print, line input, standard streams, explicit I/O errors.
- `std.string`: length, slicing boundaries, search, split/join, UTF-8 validation, formatting.
- `std.array`: length, append, capacity, safe indexing helpers.
- `std.math`: common numeric functions and constants with documented domains.
- `std.fs`: path operations, read/write files, metadata, directory iteration.
- `std.path`: platform-neutral path manipulation.
- `std.process`: arguments, environment, exit status, child processes.
- `std.time`: monotonic timing and duration operations.
- `std.test`: assertions, test discovery, deterministic test reports.
- `std.net`: only after cross-platform socket APIs and error types are stable.

The standard library must state whether each API is portable, how errors are represented, and which resource cleanup is guaranteed. Platform-specific extensions belong in clearly named modules.

## Toolchain requirements

The `ylang` command should eventually provide:
- `check`, `build`, `run`, `test`, `fmt`, `doc`, `pkg`, `clean`, and `--version`.
- A reproducible project manifest and lockfile with pinned dependency versions and hashes.
- A test runner with unit tests and integration tests.
- A formatter with stable output.
- A package manager that does not run arbitrary install scripts by default.
- A parser-backed language server and maintained VS Code/Neovim support.
- Debug builds, optimized builds, useful stack traces, and inspectable compiler IR.

## Compiler architecture

1. Lexer and parser produce a source-located AST.
2. Name resolution builds explicit module/symbol tables.
3. Type checking and ownership/dataflow analysis produce a typed IR.
4. Lowering converts typed IR into a target-independent compiler IR.
5. Optimization passes run only on validated IR.
6. A dedicated native backend emits object files and links executables; the C backend remains available until the native backend passes parity tests.
7. Runtime and standard library use a small documented ABI.

LLVM is a candidate for the first native backend because it provides a common IR, optimization infrastructure, and code generation for many targets. It is not selected blindly: prototype the smallest program, Windows linking, debug info, and distribution cost before committing. LLVM itself is a large dependency, so the project must decide between system LLVM, prebuilt toolchain packages, or an optional backend.

## Quality gates for a 2.0 release

A feature is not done until:
- Positive and negative compiler tests cover it.
- Runtime tests cover boundary cases and allocation/I/O failure behavior where applicable.
- Sanitizers and fuzzing cover parser/compiler input paths.
- GCC/Clang bootstrap builds pass on Linux; native backend tests pass on Linux and Windows.
- Windows tests compile and execute generated native programs, not just the compiler.
- VS Code and Neovim syntax/tooling match the published grammar.
- Documentation describes actual behavior and limitations.
- Dependency licenses, versions, update procedure, and known security advisories are reviewed.

## Release strategy

Keep 1.0.0 stable on `main`. Implement and test 2.0 features on development branches. Release 2.0 only after the language spec, standard library, compiler, CLI, examples, editor tooling, Linux/Windows CI, and migration notes agree. Do not promise a calendar date before these gates pass.

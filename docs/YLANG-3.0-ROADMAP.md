# YLang 3.0 — Development Roadmap

Status: active development plan. This document is a target specification, not a claim that any listed feature is implemented or stable.

The recursively decomposed, task-by-task implementation checklist is maintained in [YLANG-3.0-IMPLEMENTATION-PLAN.md](YLANG-3.0-IMPLEMENTATION-PLAN.md). Use that document to select small implementation leaves and track validation separately from code completion.

## Release objective

YLang 3.0 aims to become a fast, safe, production-oriented compiled language with first-class Linux, Windows, and macOS support; a shared compiler IR; mature LLVM and NASM backends; a comprehensive standard library; and dependable developer tooling.

The release must preserve the documented 2.2.x behavior unless a deliberate, documented 3.0 language migration is approved. The 3.0 work happens on `ylang-3.0-dev`; do not publish a 3.0 tag, GitHub Release, package, or stable website claim until every release gate below passes and the project owner explicitly approves publication.

## Non-negotiable engineering rules

1. Correctness before performance claims. Never claim YLang is the fastest language without independent, reproducible evidence across representative workloads and hardware.
2. One semantic frontend. Parsing, name resolution, type checking, ownership checking, and diagnostics are shared by all backends.
3. A typed, backend-neutral IR is the contract between frontend and code generators.
4. Backend capability matrices are explicit. Unsupported constructs fail with source-located diagnostics; no silent fallback or semantic differences.
5. Differential tests compare stdout, stderr/error class, exit status, and relevant observable side effects across all backends that claim support.
6. No safe use-after-free, double-free, dangling reference, invalid aliasing, or uninitialized read. Unsafe and FFI boundaries must be explicit.
7. Keep C backend compatibility during migration. Remove or demote it only after an approved compatibility plan and parity evidence.
8. Every public standard-library API has tests, documentation, ownership/error semantics, and platform notes.
9. Experimental labels are removed only after implementation, CI coverage, documentation, and owner acceptance—not to make the release look complete.
10. Never modify stable release markers or publish automatically from this development branch.

## Language design targets

### Core syntax and types
- Adopt `fn` as the canonical function declaration keyword, with a documented migration/deprecation path for `function`.
- Establish the new variable declaration and mutability model, targeting `let` for immutable bindings and `let mut` for mutable bindings.
- Explicit types with practical local type inference.
- Generics for functions, structs, enums, and standard collections.
- Traits/type constraints with coherent implementation and diagnostics.
- Payload enums, exhaustive pattern matching, destructuring, and nested patterns.
- Tuples, slices, ranges, iterators, closures, and first-class function values.
- Option<T> and Result<T, E>, with a defined propagation operator and exhaustive handling.
- Named/default arguments only if their interaction with overload resolution and ABI is specified.
- Compile-time constants and bounded compile-time evaluation; macros/reflection require a separate safety and complexity RFC.
- Deterministic evaluation order and explicit integer overflow/division semantics.

### Memory and safety
- Ownership and moves for owned values.
- Shared immutable and exclusive mutable borrowing with flow-sensitive conflict/lifetime checking.
- Deterministic destruction on every control-flow exit.
- Safe slices and collections with bounds guarantees.
- Explicit unsafe blocks for raw pointers, pointer arithmetic, and low-level operations.
- Allocator API, arena allocation, and scoped allocation without a mandatory tracing garbage collector.
- FFI contracts must describe ownership, lifetimes, layout, and thread-safety.

### Concurrency
- Threads, atomics, mutexes, read/write locks, channels, and thread pools.
- Async/await with a documented executor and cancellation model.
- Structured concurrency, task joining, timeouts, and error propagation.
- Data-race prevention for safe code; no promise of automatic deadlock freedom.

## Compiler and backend architecture

### Shared frontend and IR
- Keep lexer/parser/sema as the single source of language semantics.
- Add typed SSA IR with explicit basic blocks, terminators, values, types, source spans, calling conventions, and ownership/drop information.
- Verify IR invariants before optimization and before backend lowering.
- Add reusable optimization passes and pass-level tests.
- Preserve source locations through optimization for diagnostics and debugging.

### LLVM backend
- Generate LLVM IR from the shared typed IR.
- Support native object/executable generation, target triples, data layouts, debug information, and platform linkers.
- Integrate verified optimization pipelines for O0, O1, O2, O3, size, LTO, and optional PGO.
- Support Linux ELF, Windows COFF/PE, and macOS Mach-O where LLVM toolchains are available.
- Test target-feature selection and runtime CPU feature detection; never emit unsupported instructions for the selected target.

### NASM backend
- Build a production-quality x86-64 backend from the shared IR.
- Implement instruction selection, register allocation, stack frames, spills, calling conventions, relocations, object generation, and debug metadata.
- Support System V AMD64 ABI and Windows x64 ABI.
- Add scalar, floating-point, and progressively SIMD lowering (SSE2/AVX/AVX2 only with feature checks).
- Keep architecture-specific code isolated. NASM is not an ARM64 backend.
- Require the same semantic and differential tests as LLVM for every shared supported feature.

### C backend
- Retain as a compatibility/reference backend during the 3.0 transition.
- Route through the shared IR where practical, rather than maintaining separate language semantics.
- Keep GCC/Clang parity and sanitizer tests until a future deprecation is explicitly approved.

## Performance objectives

The goal is to compete with C, C++, Rust, and Zig on representative native workloads—not to promise universal superiority.

- Release builds use carefully measured optimization profiles.
- Optimize allocations, data layout, cache locality, bounds checks proven redundant, inlining, devirtualization, loop transformations, and vectorization.
- Support LTO and PGO where the selected backend supports them.
- Keep runtime startup and mandatory runtime dependencies minimal.
- Benchmark compiler time, runtime, memory, binary size, startup, and multithreaded scaling separately.
- Use independent benchmark suites and prevent dead-code elimination from invalidating tests.
- Record CPU, OS, compiler versions, flags, warmups, iterations, median, spread/P95, and output validation.
- Performance changes require repeatable gains without correctness, memory, or compile-time regressions.

## Standard library target

The standard library should be coherent and portable, with platform-specific implementations behind stable interfaces.

### Core
- `std::collections`: Vec, VecDeque, HashMap, HashSet, BTreeMap, BTreeSet, BinaryHeap.
- `std::string`, `std::str`, `std::unicode`, `std::fmt`, `std::convert`, `std::iter`, `std::hash`.
- `std::option`, `std::result`, `std::cmp`, `std::ops`, `std::any`.

### System and I/O
- `std::fs`, `std::io`, `std::path`, `std::env`, `std::process`, `std::os`, `std::thread`, `std::sync`, `std::atomic`, `std::alloc`, `std::mem`, `std::ffi`.

### Networking and data
- `std::net`, `std::http`, `std::tls`, `std::url`, `std::websocket`, `std::dns`.
- `std::json`, `std::toml`, `std::csv`, `std::xml`, serialization interfaces.
- SQLite integration as an official optional module/package; database drivers must not be required by the core compiler.

### Math, time, and security
- `std::math`, `std::random`, `std::time`, `std::datetime`, `std::timezone`, `std::decimal`, `std::bigint`, `std::units`.
- Cryptographic functionality should use vetted platform or upstream implementations; do not invent cryptographic algorithms. Provide secure random, hash, TLS, and password-hashing APIs only with reviewed dependencies and threat models.
- `std::regex`, `std::encoding`, `std::archive`.

### Application development
- Stabilize GUI and Game2D APIs with a clear event loop and resource ownership.
- Optional image, audio, window, input, graphics, and terminal UI modules.
- Keep 3D graphics, hot reload, and advanced multimedia as optional ecosystem packages unless they meet the same portability and maintenance bar.

Modules may be implemented in YLang, C, or carefully selected dependencies. The public API and behavior are the contract; rewriting mature dependencies from scratch is not a goal.

## Toolchain and ecosystem

- CLI: build, run, check, test, bench, fmt, lint, doc, clean, watch, profile, audit.
- YSPM integration remains distinct from project dependency resolution.
- Project package manager: semantic versions, deterministic resolution, lockfiles, workspaces, registries, offline cache, checksums/signatures, git dependencies, and audit metadata.
- LSP, VS Code, Neovim, and Zed support; formatting, diagnostics, completion, navigation, refactoring, and debugger integration.
- Debug symbols and source-level debugging for supported backends and platforms.
- Cross-compilation and reproducible builds.
- Fuzzing for lexer/parser/IR, sanitizer runs, static analysis, and supply-chain checks.

## Platform support contract

- Linux x86-64: primary reference target.
- Windows x86-64: first-class target, not experimental; native compiler builds, executable generation, filesystem/process/network APIs, package installation, tests, and documented toolchain support.
- macOS: first-class target with native toolchain, linker, filesystem, networking, GUI dependencies, and CI coverage.
- ARM64: support only when each backend and standard-library module has real CI and runtime coverage; do not imply NASM support.

## Milestone order

1. Audit current 2.2.0 implementation and write feature/capability inventory.
2. Freeze 3.0 language RFCs: `fn`, bindings, generics, traits, enums/patterns, errors, memory semantics.
3. Stabilize frontend invariants and regression/fuzz coverage.
4. Implement typed IR and verifier.
5. Lower the existing supported language subset through IR; preserve C backend parity.
6. Implement LLVM backend and platform object/link pipeline.
7. Expand NASM x86-64 backend and Windows ABI.
8. Implement memory safety, generics, traits, and advanced language constructs incrementally.
9. Expand standard library and async/concurrency with ownership-aware APIs.
10. Complete Windows/macOS/Linux CI, packaging, LSP/debugger, and docs.
11. Run full differential, fuzz, sanitizer, security, and compatibility suites.
12. Run final performance campaign against C/Rust/Zig and optimize based on measured bottlenecks.
13. Owner acceptance and explicit stable publication authorization.

## Release gates

YLang 3.0 is not stable until all of these are true:

- Every feature advertised as stable has implementation and positive/negative tests.
- No core feature is marked experimental.
- LLVM and NASM capability matrices are published and tested; unsupported targets/features fail clearly.
- Windows x86-64 is covered by native CI and end-to-end build/run tests.
- Linux and macOS regression suites pass.
- Sanitizers and fuzz smoke tests pass on supported CI runners.
- Cross-backend differential tests pass for all overlapping features.
- Standard-library public APIs have docs and tests; optional dependencies and security boundaries are documented.
- Package manager lockfile/resolution integrity and release artifacts are verified.
- Benchmarks are reproducible and published without unsupported "fastest language" claims.
- Migration guide from 2.2.x is complete.
- Owner explicitly approves the exact tested commit for stable publication.

Until then, the branch remains development-only and 2.2.x remains the stable release.

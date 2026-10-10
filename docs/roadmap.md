# YLang 2.0 Development Roadmap

This roadmap describes the intended release, not features already implemented. Stable YLang 1.0.0 remains on `main`.

## Release principle

Prioritize the core semantics and test harness over a long list of shallow features. Each feature must include the compiler, tests, documentation, and editor grammar. Keep Windows CI running continuously and run executable smoke tests on every platform.

## Stage 0 — Stabilize the development foundation

- [ ] Review and stabilize scalar `&T` / `&mut T` borrowing.
- [ ] Ensure diagnostics reject all mismatched modes and conflicting aliases.
- [ ] Fix parser/codegen edge cases before adding heap-backed types.
- [ ] Add fuzz targets for lexer/parser and sanitizer CI.
- [ ] Define versioning and compatibility policy for 2.0.

## Stage 1 — Ownership and deterministic cleanup

- [ ] Define string representation, UTF-8 contract, and clone/move semantics.
- [ ] Add move-state analysis and reject use-after-move.
- [ ] Replace process-lifetime f-string allocations with deterministic ownership.
- [ ] Generate cleanup on all control-flow exits and error paths.
- [ ] Test early returns, nested scopes, loops, and repeated formatting.
- [ ] Keep references call-scoped until lifetimes are implemented.

## Stage 2 — Arrays

- [ ] Add `T[]` types and typed literals.
- [ ] Add indexing reads/writes with bounds checks.
- [ ] Add `len`, append, capacity, and explicit clone/move rules.
- [ ] Define empty-array typing and allocation failure behavior.
- [ ] Test invalid indexes, zero-length arrays, overflow, and memory cleanup.

## Stage 3 — Essential language features

- [ ] Line input and typed conversion APIs with EOF/error behavior.
- [ ] Range-based `for` iteration.
- [ ] Match/enum or another explicit sum-type design for robust error handling.
- [ ] Multi-file modules/imports and visibility.
- [ ] Structs/records and methods only after value layout and ownership are clear.
- [ ] Generics only after monomorphization/type-checking design and compile-time diagnostics are tested.

## Stage 4 — Standard library

- [ ] `std.io`: output, line input, streams, I/O errors.
- [ ] `std.string`: UTF-8 operations, formatting, split/join/search.
- [ ] `std.array`: safe collections and capacity operations.
- [ ] `std.math`: numeric functions and documented domains.
- [ ] `std.fs` and `std.path`: portable filesystem APIs.
- [ ] `std.process`: args, environment, exit status, child processes.
- [ ] `std.time`: monotonic clocks and durations.
- [ ] `std.test`: unit and integration test runner.
- [ ] Add networking and serialization only after error/result and ownership models stabilize.

## Stage 5 — Dedicated compiler backend

- [ ] Prototype LLVM IR emission for constants, arithmetic, functions, conditionals, loops, and strings.
- [ ] Produce native Linux and Windows executables.
- [ ] Test debug information, link behavior, runtime ABI, and distribution size.
- [ ] Keep the C backend as a reference until output parity and regression tests pass.
- [ ] Add explicit target selection and optimization levels.
- [ ] Never run optimization passes over invalid or ill-typed IR.

## Stage 6 — Modules, packages, and dependency security

- [ ] Define project manifest and lockfile.
- [ ] Pin dependencies with hashes and license metadata.
- [ ] Default to no arbitrary package install scripts.
- [ ] Add dependency audit/update workflow and reproducible builds.
- [ ] Support local path dependencies and a small trusted registry before broad registry support.

## Stage 7 — Tooling and developer experience

- [ ] Add `ylang run`, `test`, `fmt`, `doc`, and `pkg`.
- [ ] Improve diagnostics with suggestions and related locations.
- [ ] Upgrade the language server from regex-based checks to parser/semantic-model-backed analysis.
- [ ] Keep VS Code TextMate, Tree-sitter, Neovim, and LSP consistent.
- [ ] Add project templates and end-to-end examples.

## Stage 8 — Release gates

- [ ] Linux GCC and Clang builds, tests, sanitizers, and fuzz smoke tests.
- [ ] Windows MSYS2 UCRT64 and native PowerShell build/test.
- [ ] Windows native-backend output executes correctly.
- [ ] Editor extension packaging and language-server tests on Linux/Windows.
- [ ] Tree-sitter grammar tests and Neovim install checks.
- [ ] Benchmark against a small documented suite, with compiler versions and flags recorded.
- [ ] Publish spec, standard library docs, migration guide, dependency/license inventory, and known limitations.
- [ ] Only then tag YLang 2.0.0.

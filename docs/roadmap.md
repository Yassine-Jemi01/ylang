# YLang Development Roadmap

This is the working roadmap for `v2/core`. It records current scope and next milestones; it is not a stable-release promise. `main` remains the YLang 1.0.0 contract until a future release is intentionally prepared.

## Working principles

- Keep the language core small enough to reason about and the diagnostics actionable.
- Make ownership transfer and expensive copies explicit.
- Keep platform-specific code out of language semantics.
- Add regression tests with each feature and retain Windows CI as a portability gate.
- Prefer the C standard library for basic runtime facilities until a third-party dependency provides a clear benefit. Keep editor tools aligned with the compiler grammar.

## Current preview

- Dedicated C front-end: lexer, parser, semantic/type checking, diagnostics, and C17 code generation.
- Scalars, functions, conditions, infinite loop, C-style `for`, typed one-dimensional arrays, indexing and bounds checks.
- Built-ins for line input, length, explicit clone, append, and print.
- Initial scalar borrow parameters (`&T`, `&mut T`) and initial move/use-after-move checks for strings and arrays.
- GCC and Clang output backends, Linux CI, and Windows/MSYS2 CI.

## Phase 1 — Stabilize memory and safety

1. Convert move-state checks to a real control-flow dataflow analysis with correct merge behavior for branches and loops.
2. Expand and stress-test the new code-generation cleanup paths for `return`, `break`, `continue`, function parameters, nested scopes, and all owned array/string cases.
3. Correct and expand tests for strings in arrays, clone behavior, repeated append, reinitialization after move, and allocation cleanup.
4. Run sanitizer and malformed-source tests; document unsupported cases rather than silently accepting them.

## Phase 2 — Standard library

1. Add portable text-file APIs with clear error results and size limits.
2. Introduce a consistent error-value model (for example a typed `Result` or equivalent) instead of using process termination for every expected failure.
3. Add string operations, paths, and numeric conversion functions with explicit byte/Unicode semantics.
4. Evaluate external open-source libraries only for features that benefit from them; pin versions, review licenses, and run Linux/Windows tests. Do not add a dependency just for branding.

## Phase 3 — Multi-file programs

Design imports and modules before implementation: file resolution, name visibility, duplicate symbols, cycles, compile order, and diagnostics. Add tests for missing modules and circular imports. Build an incremental module model before proposing package management.

## Phase 4 — Performance

Add repeatable benchmarks for arithmetic, loops, array indexing/append, string formatting, and function calls. Measure release builds with GCC and Clang. Preserve runtime checks unless the compiler can prove they are redundant. Track compile time and executable size as well as runtime.

## Phase 5 — Compiler independence

The current compiler has a dedicated YLang front-end but emits C17 and delegates machine-code generation to GCC/Clang. A direct LLVM or custom native backend is a separate strategic decision; it must be justified by measurable benefits and should not arrive before the language semantics and standard library settle.

## Phase 6 — Platform and editor release review

- GCC and Clang regression tests on Linux.
- MSYS2 UCRT64 Windows build, full regression suite, PowerShell build/smoke test, and generated Windows executable runs.
- VS Code packaging/LSP checks on Linux and Windows.
- Regenerated Tree-sitter parser and corpus tests; Neovim installer and parser checks.
- Sanitizers, ownership regression tests, docs consistency, versioning, and a review of known safety limitations.

## Explicitly not ready to promise

Rust-level memory safety, MSVC support, full Unicode string semantics, nested arrays, generics, classes, exceptions, async/concurrency, modules/imports, broad filesystem/network libraries, and a direct machine-code backend.

# YLang Development Roadmap

This is the working roadmap for YLang 2.0 development. It records current scope and next milestones; it is not a stable-release promise. `main` remains the stable YLang 1.1.x line until a future release is intentionally prepared.

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

An experimental local-file import resolver now supports lines such as `from utils.yl import system_data()`. It resolves paths relative to the importing file, loads nested dependencies once, rejects cycles and missing symbols, applies source limits, and maps diagnostics back to the imported file. Regression tests cover nested imports, missing modules/symbols, cycles, and source-located type errors.

Remaining work:
1. Enforce per-module symbol visibility rather than flattening all imported declarations into one global namespace.
2. Decide duplicate-name rules, aliases, public/private declarations, and module-qualified references.
3. Add a proper import AST/module graph and incremental build caching.
4. Design external-library/package metadata after the language-level module model is stable.

## Phase 4 — HTTP/JSON API support

An initial synchronous HTTP/HTTPS client and JSON helper runtime now exist on `v2/http-api-client`. The client supports common REST verbs, custom headers, bounded response bodies, TLS verification, timeouts, and status/error accessors. JSON helpers support checked scalar properties, raw nested objects/arrays, array length/items, and string quoting. Linux GCC and Clang integration tests exercise the HTTP client against a local server and JSON behavior without an external JSON dependency.

Remaining work before stabilization:
1. Replace the process-global HTTP status/error accessors with a response value that carries status, body, and transport errors.
2. Expand JSON edge-case tests, including invalid syntax, numeric range, duplicate keys, Unicode/surrogate handling, depth and size limits.
3. Make `http` and `json` importable modules after the multi-file import model is stable.
4. Add timeouts, TLS/connection-failure, oversized body, redirect, and header-injection tests; keep verifying Windows CI and generated application behavior.
5. Document the optional libcurl dependency and avoid requiring it when a program does not use HTTP.
Hosting REST endpoints is a later milestone, not part of the initial client.

## Phase 5 — Performance

Add repeatable benchmarks for arithmetic, loops, array indexing/append, string formatting, function calls, JSON parsing, and HTTP response handling with a local mock server. Measure release builds with GCC and Clang. Preserve runtime checks unless the compiler can prove they are redundant. Track compile time and executable size as well as runtime.

## Phase 6 — Compiler independence

The current compiler has a dedicated YLang front-end but emits C17 and delegates machine-code generation to GCC/Clang. A direct LLVM or custom native backend is a separate strategic decision; it must be justified by measurable benefits and should not arrive before the language semantics and standard library settle.

## Phase 7 — Platform and editor release review

- GCC and Clang regression tests on Linux.
- MSYS2 UCRT64 Windows build, full regression suite, PowerShell build/smoke test, and generated Windows executable runs.
- VS Code packaging/LSP checks on Linux and Windows.
- Regenerated Tree-sitter parser and corpus tests; Neovim installer and parser checks.
- Sanitizers, ownership regression tests, docs consistency, versioning, and a review of known safety limitations.

## Explicitly not ready to promise

Rust-level memory safety, MSVC support, full Unicode string semantics, nested arrays, generics, classes, exceptions, async/concurrency, modules/imports, and a direct machine-code backend remain unsupported until their respective milestones and tests are complete. Full typed JSON values, importable HTTP/JSON modules, a stable response/error model, and an HTTP server remain unfinished.

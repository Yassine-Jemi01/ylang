# YLang Development Roadmap

This is a working plan for development on `dev/lsp-foundation`. It is not a release promise. The stable `main` branch remains the YLang 1.0.0 contract until a future release is intentionally prepared.

## Working rule

Build the language core first and keep operating-system details out of language semantics. Add focused Linux regression tests with each feature so bugs are caught while the change is small. Keep the existing Windows CI smoke checks as an early portability warning; run a broader Windows/editor compatibility pass after the planned language features have settled.

A syntax or semantic feature is not complete until its specification, diagnostics, tests, and editor grammar are updated. The LSP may temporarily lag while syntax is experimental, but that limitation must be explicit.

## Phase 1 — Ownership and borrowing

The first scalar-borrowing subset is now on the development branch. Stabilize its semantics and regression tests first:

- `&T` shared borrows and `&mut T` exclusive borrows at function-call boundaries.
- Reject parameter-mode mismatches, conflicting borrows, and reads/writes during a mutable borrow.
- Define move semantics for owned strings, use-after-move detection, explicit cloning, and cleanup on all control-flow exits.
- Keep strings and arrays out of the borrowable type set until their ownership and lifetime rules are implemented.

Do not promise Rust-like memory safety without a complete design and evidence.

## Phase 2 — Arrays and iteration

After ownership/move semantics are implemented and tested, add one-dimensional typed arrays in small increments:

1. Array type syntax and array literals.
2. Index reads and writes, with compile-time type checking.
3. Length access and defined behavior for invalid indexes.
4. Tests for empty arrays, initialization, const/mutable behavior, nested scopes, and error cases.
5. A useful iteration form (such as a range-based `for`), designed around the settled array contract.

Update the parser, AST, semantic analysis, C generator/runtime, language specification, Tree-sitter grammar, and VS Code language support together or record any temporary editor limitations.

## Phase 3 — Input and core library

Add a portable input path using standard C facilities where possible, avoiding operating-system-specific code in language semantics. Define line input, end-of-file behavior, and conversion failures before exposing APIs. Add examples and tests for successful input, empty input, malformed values, and EOF.

## Phase 4 — Multi-file programs

Design imports/modules before implementation. Specify module naming and file resolution, duplicate names, visibility, circular imports, and how the CLI builds multiple files. Then add tests for valid imports, missing modules, duplicate symbols, and cycles. Do not start with a package manager before multi-file compilation behavior is clear.

## Phase 5 — Tooling catches up

Improve the LSP from its current lightweight analysis toward parser-backed diagnostics and scope-aware symbols. Then expand completion, hover, go-to-definition, and cross-file navigation as the compiler/module model allows. Keep TextMate highlighting and Tree-sitter grammar consistent with the actual language specification.

## Phase 6 — Full compatibility and release review

When core feature work is stable, perform a deliberate compatibility pass:

- Linux with GCC and Clang.
- Windows with MSYS2 UCRT64 / MinGW-w64, the native PowerShell build script, and generated executable smoke tests.
- VS Code extension packaging and language-server checks on Linux and Windows.
- Tree-sitter grammar tests plus Neovim installation checks on supported platforms.
- Sanitizer runs and regression checks for invalid input, bounds errors, integer overflow, division by zero, and generated-code failures.

Do not call a platform fully supported just because the compiler itself builds there; the CLI, generated programs, editor integration, docs, and automated checks all need to agree.

# YLang 2 Preview — Compiler Architecture

This page documents the current development branch. YLang 1.0.0 remains the stable contract on `main`; this branch is versioned `2.0.0-dev`.

## Front-end

1. **Lexer (`src/lexer.c`)** turns source text into positioned tokens.
2. **Parser (`src/parser.c`)** builds the AST for declarations, functions, expressions, arrays, indexing, control flow, and borrow expressions.
3. **Semantic analysis (`src/sema.c`)** checks names, initialization, type consistency, function calls, array element types, borrow conflicts, a first-pass owned-value move state, and loop rules.
4. **C code generator (`src/codegen.c`)** emits C17 for the checked AST and includes YLang runtime helpers for arrays, input, checked integer arithmetic, and formatting.
5. **Driver (`src/driver.c`)** handles source loading, command orchestration, generated-C temporary files, and native compiler invocation. It uses POSIX process support on Linux and native process APIs on Windows.
6. **CLI (`src/main.c`)** exposes `check`, `build`, `emit-c`, and the narrow `fix` command.

The internal AST/compiler model lives in `include/ylang/compiler.h` and `src/internal.h`. Public lexer declarations are in `include/ylang/lexer.h`.

## Back-end boundary

This is a custom YLang compiler front-end, but the native back-end is currently generated C17 compiled by GCC or Clang. There is no separate LLVM IR or direct machine-code backend yet. Windows CI uses GCC from MSYS2 UCRT64/MinGW-w64; MSVC is not supported by the generated runtime at this stage.

Generated programs run as normal native processes with the caller's permissions. Compilation does not sandbox a program.

## Runtime model

- Scalars are copied by value.
- Strings are immutable pointers to NUL-terminated byte strings.
- Arrays are typed, one-dimensional values represented by a (data, len, cap) structure.
- Named strings and arrays are moved in supported ownership contexts. `clone` creates an explicit copy; array indexes are checked at runtime.
- Runtime allocations are tracked. Local strings and arrays are dropped on normal block exit, with process-exit cleanup as a safety net. Early exits and owned string elements still need more deterministic cleanup before a stable safety claim is appropriate.
- The first borrow-checking subset supports `&T` and `&mut T` for `int`, `float`, `bool`, and `char` function parameters only. References cannot be stored in locals or returned.

## Diagnostics and testing

Compiler errors include codes and source locations. A source with reported errors must not be built into a successful native executable. Regression tests run in Linux CI with GCC and Clang, and Windows CI compiles the compiler and runs the suite under MSYS2. Tree-sitter generation/tests and VS Code extension packaging run in CI as well.

Tests and sanitizers are useful bug-finding tools, not a proof of memory safety. The ownership checker, control-flow state analysis, and generated runtime need continued review.

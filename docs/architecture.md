# YLang Compiler Architecture

YLang 1.0.0 uses a conventional multi-stage compiler pipeline:

1. **Lexer (`src/lexer.c`)** — turns source text into positioned tokens.
2. **Parser (`src/parser.c`)** — builds the program representation and emits syntax diagnostics.
3. **Semantic analysis (`src/sema.c`)** — resolves names, validates initialization and constant assignment, and checks types.
4. **C code generator (`src/codegen.c`)** — writes C for the checked program, including runtime helpers for supported checked arithmetic operations.
5. **Driver (`src/driver.c`)** — reads the source, orchestrates checking, writes generated C, and invokes GCC or Clang without going through a shell.
6. **CLI (`src/main.c`)** — parses user commands and their options.

The internal compiler model is declared in `src/internal.h`; the public-facing compiler and token declarations are in `include/ylang/`.

## Diagnostic policy

Compiler errors are intended to be stable, user-facing output. Changes should preserve useful codes where practical and include regression tests for source locations and hints. A check failure must not be reported as a native build failure. Code generation is only entered after parsing and semantic analysis report no errors.

## Native backend

The v1.0 backend generates C17 and invokes the selected native compiler (`gcc` by default or `clang` when selected). Linux builds use GNU linker RELRO/NOW hardening and `_FORTIFY_SOURCE`; macOS builds omit GNU/Linux-specific flags and use the platform linker. Generated programs are ordinary native executables, not sandboxed. For native builds, generated C is placed in a private temporary directory and removed after the native compiler exits. This avoids clobbering a project file and avoids collisions between concurrent build invocations. `emit-c` writes to the requested path, or `build/ylang-generated.c` if no output path is given.

## Scope

This architecture documents the current implementation, not a promise of features outside `docs/language-spec.md`. Arrays, pointers/references, classes, exception syntax, modules, and a finalized memory model are intentionally outside the 1.0.0 feature set.

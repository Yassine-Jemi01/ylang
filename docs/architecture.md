# YLang Compiler Architecture

YLang 1.0.0 uses a conventional multi-stage compiler pipeline:

1. **Lexer (`src/lexer.c`)** — turns source text into positioned tokens.
2. **Parser (`src/parser.c`)** — builds the program representation and emits syntax diagnostics.
3. **Semantic analysis (`src/sema.c`)** — resolves names, validates initialization and constant assignment, and checks types.
4. **Module loader (`src/module.c`)** — recursively loads imported files, detects cycles, resolves named imports, and merges module ASTs while retaining per-module visibility.
5. **C code generator (`src/codegen.c`)** — writes C for the checked program, including runtime helpers for supported checked arithmetic operations.
6. **Driver (`src/driver.c`)** — orchestrates module loading and checking, writes generated C, and invokes GCC or Clang without going through a shell.
7. **CLI (`src/main.c`)** — parses user commands and their options.

The internal compiler model is declared in `src/internal.h`; the public-facing compiler and token declarations are in `include/ylang/`.


## Experimental NASM backend (Linux x86-64)

The optional emit-asm command writes NASM syntax for a deliberately limited
subset of YLang. Unlike the default C backend, the generated program uses the
Linux x86-64 system-call ABI for output and process exit, so a generated
program can be assembled with NASM and linked directly with ld without
GCC/Clang or a C runtime:

    ./build/ylang emit-asm tests/asm-basic.yl -o build/asm-basic.asm
    nasm -f elf64 build/asm-basic.asm -o build/asm-basic.o
    ld -o build/asm-basic build/asm-basic.o
    ./build/asm-basic

For the same steps in one command, use build-asm:

    ./build/ylang build-asm tests/asm-basic.yl -o build/asm-basic
    ./build/asm-basic

The initial subset supports scalar int, bool, char, and string values,
local variables, scalar globals, fixed-size local arrays, initialized global
arrays, up to six scalar function parameters, user-defined function calls,
integer arithmetic and comparisons, short-circuit boolean `and`/`or`,
if/else, loops, break, continue, return, and print. Global initializers are
evaluated in the startup stub before `main()`. Array indexing has runtime
bounds checks and `length(array)` is lowered to a compile-time constant.
Integer overflow and invalid integer division terminate with status 70.

The experimental backend still rejects array parameters, nested arrays, floats,
f-strings, string comparisons, and standard-library or conversion calls. Named
imports are resolved by the shared module loader, and the NASM backend emits the
merged, semantically checked module graph as one executable. Global arrays require fixed-size literal initializers.
Scalar and array globals are stored in the generated program's BSS and
initialized by its startup stub. The standard build command continues to use
the established C backend. The separate `build-asm` command runs NASM and
`ld` for supported programs; `emit-asm` writes assembly for inspection.
The experimental backend targets Linux x86-64 only.
## Diagnostic policy

Compiler errors are intended to be stable, user-facing output. Changes should preserve useful codes where practical and include regression tests for source locations and hints. A check failure must not be reported as a native build failure. Code generation is only entered after parsing and semantic analysis report no errors.

## Native backend

The v1.0 backend generates C17 and invokes the selected native compiler (`gcc` by default or `clang` when selected). Linux builds use GNU linker RELRO/NOW hardening and `_FORTIFY_SOURCE`; macOS builds omit GNU/Linux-specific flags and use the platform linker. Generated programs are ordinary native executables, not sandboxed. For native builds, generated C is placed in a private temporary directory and removed after the native compiler exits. This avoids clobbering a project file and avoids collisions between concurrent build invocations. `emit-c` writes to the requested path, or `build/ylang-generated.c` if no output path is given.

## Scope

This architecture documents the current implementation, not a promise of features outside `docs/language-spec.md`. Arrays, pointers/references, classes, exception syntax, and a finalized memory model are intentionally outside the original 1.0.0 feature set. Named imports are supported by the default C backend; the NASM backend remains a limited experimental subset.

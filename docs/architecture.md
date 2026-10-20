# YLang Compiler Architecture

YLang 2.0.0 uses a conventional multi-stage compiler pipeline. The stable core compiles through generated C and GCC/Clang; named imports, structs, enums, GUI/Game2D, and the NASM backend are explicitly experimental extensions.

1. **Lexer (`src/lexer.c`)** — turns source text into positioned tokens.
2. **Parser (`src/parser.c`)** — builds the program representation and calls shared diagnostics when syntax is invalid.
3. **Frontend utilities** — `src/arena.c` owns arena allocation and arena-backed vectors; `src/strbuf.c` owns generated-text buffers; `src/diagnostics.c` formats source-located errors and warnings.
4. **Semantic analysis (`src/sema.c`)** — resolves names, validates initialization and constant assignment, and checks types.
5. **Module loader (`src/module.c`)** — recursively loads imported files, detects cycles, resolves named imports, and merges module ASTs while retaining per-module visibility.
6. **C code generator (`src/codegen.c`)** — writes C for the checked program, including runtime helpers for supported checked arithmetic operations.
7. **Driver (`src/driver.c`)** — orchestrates module loading and checking, writes generated C, and invokes GCC or Clang without going through a shell.
8. **CLI (`src/main.c`)** — parses user commands and their options.

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
local variables, up to six scalar function parameters, user-defined function
calls, integer arithmetic and comparisons, if/else, loops, break,
continue, return, and print. Integer overflow and invalid integer
division terminate the program with status 70.

The experimental backend currently rejects global declarations, arrays,
floats, f-strings, string comparisons, and/or, standard-library or
conversion calls, and multi-file imports. The standard build command continues to use the established C backend.
The separate build-asm command runs NASM and ld for supported programs;
emit-asm writes the assembly source for inspection. The experimental backend
is Linux x86-64 specific and is not supported on macOS or other architectures.

## Diagnostic policy

Compiler errors are intended to be stable, user-facing output. Changes should preserve useful codes where practical and include regression tests for source locations and hints. A check failure must not be reported as a native build failure. Code generation is only entered after parsing and semantic analysis report no errors.

## Native backend

The v1.0 backend generates C17 and invokes the selected native compiler (`gcc` by default or `clang` when selected). Linux builds use GNU linker RELRO/NOW hardening and `_FORTIFY_SOURCE`; macOS builds omit GNU/Linux-specific flags and use the platform linker. Generated programs are ordinary native executables, not sandboxed. For native builds, generated C is placed in a private temporary directory and removed after the native compiler exits. This avoids clobbering a project file and avoids collisions between concurrent build invocations. `emit-c` writes to the requested path, or `build/ylang-generated.c` if no output path is given.

## Scope

This architecture documents the current implementation, not a promise of features outside `docs/language-spec.md`. Arrays, pointers/references, classes, exception syntax, and a finalized memory model are intentionally outside the original 1.0.0 feature set. Named imports are supported by the default C backend; the NASM backend remains a limited experimental subset.

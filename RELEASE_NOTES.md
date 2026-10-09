# YLang 1.0.0 — Stable Release

YLang 1.0.0 is the first stable release of the language subset documented in `docs/language-spec.md`.

## Highlights

- Compiled workflow: YLang source → checked AST → generated C → native executable.
- `ylang check`, `ylang build`, and `ylang emit-c` commands.
- Explicit types, constants, functions, conditionals, `loop()`, and f-strings.
- Actionable source diagnostics with error codes, source excerpts, and repair hints.
- A conservative `ylang fix` command that writes to an explicitly requested output path.
- Regression tests for successful programs, diagnostics, runtime arithmetic failures, safe fixes, and GCC/Clang output parity when Clang is installed.

## Important scope note

This release is stable for the subset defined in the language specification; it is not a claim that the originally envisioned full language is complete. Arrays, pointers/references, OOP, `try`/`catch`, and a finalized memory model are not included. See the specification and README before using YLang for real projects.

## Build from source

```sh
make
make test
./build/ylang --version
```

Requires a C17 compiler, GNU Make, and GCC or Clang for generated programs.

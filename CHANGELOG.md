# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [Unreleased] — 2.0.0-dev

This development line is experimental and is not a stable release.

- Added typed one-dimensional arrays, literals, indexing, checked bounds, `len`, `clone`, and `append`.
- Added C-style `for` loops, line-based `input()`, `input_int()`, and `input_float()`.
- Added a first-pass owned-value move checker for strings/arrays and retained the scalar `&T` / `&mut T` borrow subset.
- Added regression fixtures for arrays, loop execution, input, move-after-use, type mismatches, and bounds violations.
- Updated compiler version output and the v2 preview documentation.
- Windows development support remains based on MSYS2 UCRT64 / MinGW-w64. Do not treat this preview as Rust-equivalent memory-safe; cleanup on all early exits and the ownership checker are still under development.

## [1.0.0] - 2026-10-09

First stable release of the documented YLang language subset.

- Published the v1.0 language specification and explicitly documented unsupported features.
- Stabilized the CLI commands: `check`, `build`, `emit-c`, and `fix`.
- Added a canonical `VERSION` file and versioned CLI output.
- Added standard `make install`, `make uninstall`, and `make sanitize` targets.
- Added contributor guidance, security notes, CI configuration, and release notes.
- Retained type/name/initialization diagnostics, safe typo correction, C code generation, GCC/Clang integration, and regression tests.

## Earlier development

The pre-1.0 development series introduced the lexer/parser, semantic analysis, C code generation, native compilation, source diagnostics, and a narrow `pritn` to `print` fixer. Those prototype versions are summarized here rather than treated as stable releases.

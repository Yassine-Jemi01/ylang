# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [Unreleased] — 2.0.0-dev

This development line is experimental and is not a stable release.

- Added typed one-dimensional arrays, literals, indexing, checked bounds, `len`, `clone`, and `append`.
- Added C-style `for` loops, line-based `input()`, `input_int()`, and `input_float()`.
- Added a first-pass owned-value move checker for strings/arrays and retained the scalar `&T` / `&mut T` borrow subset.
- Added regression fixtures for arrays, loop execution, input, move-after-use, type mismatches, and bounds violations.
- Added an experimental synchronous HTTP/HTTPS client for REST APIs, with GET/POST helpers, general request methods, custom headers, status/error accessors, TLS verification, timeouts, and response-size limits.
- Added an initial JSON parser/helper set for scalar property access, raw nested objects/arrays, array length/items, string quoting, and checked errors.
- Added experimental recursive local-file imports with function-name checks, cycle detection, source-relative resolution, source-unit diagnostics, and regression fixtures for nested modules and import errors.
- Hardened JSON parsing against truncated Unicode escapes and HTTP response-size overflow; HTTP redirects are returned to the caller instead of being followed automatically.
- Added local mock-server and JSON regression tests; Linux GCC/Clang builds pass. Windows CI remains a required release gate.
- Documented known limitations: HTTP response status is process-global, JSON is not yet a general typed value model, and API helpers are not yet importable modules.
- Added `ylang run`, which compiles into a temporary directory, executes the generated program, removes temporary artifacts, and preserves the program's exit status.
- Added code-generation cleanup on return, break, continue, and function exit, plus string replacement that releases a previous owned value.
- Windows development support remains based on MSYS2 UCRT64 / MinGW-w64. Do not treat this preview as Rust-equivalent memory-safe; move analysis, string-array ownership, and complete cleanup guarantees still need more testing.

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

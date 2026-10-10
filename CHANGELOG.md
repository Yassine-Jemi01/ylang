# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [1.1.0] - 2026-10-10

- Added explicit conversions: `to_float`, range-checked `to_int`, and `to_string`.
- Added `string.is_int` and `string.parse_int` for checked integer text parsing.
- Added `io.file_exists` and suppressed common generated-C unused-variable warnings in normal builds.
- Documented diagnostic codes and refreshed the language specification for 1.1.0.

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

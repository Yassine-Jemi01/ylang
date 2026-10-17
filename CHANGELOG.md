# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [2.0.0-beta.1] — prerelease candidate (not yet published)

This is the release-candidate changelog for the experimental 2.0 beta. It is not a declaration of a published GitHub release.

- Added named imports and multi-file compilation.
- Added experimental typed unit enums with exhaustive `match`.
- Added experimental typed structs, field access, and mutable field assignment.
- Expanded optional SDL2-backed GUI and 2D drawing, input, timing, and collision helpers.
- Kept the C backend as the default and the NASM backend explicitly experimental.
- Updated compiler version reporting and generated-C headers to use the canonical `VERSION` value.
- Extracted the emitted C runtime into a standalone source template embedded as bytes, avoiding oversized adjacent string literals.
- Added lexer/parser unit tests, an optional libFuzzer smoke harness, and C/NASM differential parity samples for the shared subset.
- Made the SDL2 integration smoke test explicitly skip when SDL2 development dependencies are unavailable.
- `Option<T>` and `Result<T, E>` are not implemented and are excluded from this beta.

See [draft beta notes](docs/CHANGELOG-2.0-BETA.md) for limitations and upgrade guidance. Publish only after the exact release commit passes CI and the prerelease is created intentionally.

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

# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [2.0.1] — patch release

- Fixed mutable assignment to chained nested struct fields, for example `hero.pos.x = 9;`.
- Added regression coverage for direct nested writes, compound assignment, increment, and type mismatch rejection.

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [2.0.0-beta.2] — experimental prerelease

- Consolidated the experimental native Windows x86-64 path using MinGW-w64, including native executable, multi-file import, standard-input, and SDL2 build smoke tests.
- Added the existing YSPM system package manager integration for Linux x86-64, with CI building and inspecting a `.yspkg` containing the compiler and source-level standard library.
- Added the playable Game2D Dodge example, helper module installation, and usage documentation.
- Expanded CI/release gates for the exact tested commit, the YSPM package artifact, website integrity, and editor diagnostics.
- Corrected release documentation to distinguish the Linux system package manager from project-local `.yl` imports.
- Kept the C backend as the default and NASM experimental with a restricted supported subset.
- Kept object-oriented programming, `Option<T>`, `Result<T, E>`, and a complete LLVM/native backend out of scope for this beta.

## [2.0.0-beta.1] — previous source-only prerelease

The earlier beta.1 prerelease was published without binary/package assets and remains unchanged. It introduced the initial multi-file imports, experimental typed structs and enums, GUI/Game2D helpers, compiler validation improvements, and the experimental NASM backend.

See [current beta release notes](docs/CHANGELOG-2.0-BETA.md) for limitations and upgrade guidance. Beta versions are for evaluation; do not treat them as a declaration of stable 2.0.0 compatibility.

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

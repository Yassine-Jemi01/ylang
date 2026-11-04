# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [2.3.0] — development (not released)

YLang 2.3.0 is under active development. YLang 2.2.0 remains the latest stable release.

- Removed the YTilawa application and its dedicated fixtures/build steps while retaining shared TUI, Voice, HTTP streaming, and binary I/O libraries.
- Added compiler resource limits for diagnostic count/output, import depth, module count, and aggregate project source size; root files are size-checked before allocation and embedded NUL bytes are rejected in root/imported sources.
- Fixed an HTTP response-header capacity underflow and added an exact-boundary loopback fixture.
- Added immutable raw-JSON builders with key escaping and deterministic duplicate-key replacement; invalid raw fragments are rejected.
- Added parser statement-depth recovery, code-generation recursion guards, and fail-closed C generation on emission diagnostics.
- Added `ylang help` and `ylang version` subcommands, CLI documentation, and responsive homepage/TUI design updates.
- Expanded regression fixtures for resource limits, diagnostic sanitization, parser nesting, JSON builders, HTTP headers, and CLI aliases.

All build, regression, sanitizer, fuzz, cross-platform, website/book, packaging, and benchmark gates remain pending until the validation phase. Do not change `VERSION` or publish `v2.3.0` before the acceptance checklist is complete.

See [the 2.3 development notes](docs/CHANGELOG-2.3.0.md) and [release roadmap](docs/ROADMAP-2.3.0.md).
## [2.1.0] — release candidate (not yet published)

This candidate promotes the experimental ownership work for evaluation; it does not claim a complete Rust-like memory model.

- Added experimental move-only `owned_string` values and explicit `move(name)` transfers in the C backend.
- Added deterministic generated cleanup for owned locals on normal scope exit, `return`, `break`, and `continue`.
- Added restricted immutable `borrowed_string` function parameters.
- Added diagnostics and regression coverage for invalid moves, use-after-move, unsupported storage locations, and ownership/control-flow cases.
- Existing `string` behavior remains unchanged; legacy allocations are not automatically reclaimed by this feature.
- Limitations: `owned_string` currently requires the C backend. Owned globals, arrays, and struct fields remain unsupported. General `&T`/`&mut T`, lifetime inference, and complete Rust-like memory safety are not implemented.

This is a release-preparation entry, not a publication announcement. Publish `v2.1.0` only after the release acceptance checks and CI pass on the exact candidate commit.

## [2.0.1] — patch release

- Fixed mutable assignment to chained nested struct fields, for example `hero.pos.x = 9;`.
- Added regression coverage for direct nested writes, compound assignment, increment, and type mismatch rejection.

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

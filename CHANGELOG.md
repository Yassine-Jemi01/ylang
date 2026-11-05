# Changelog

All notable changes to YLang are documented here. This project follows semantic versioning for future releases.

## [2.3.0] — development (not released)

- Made HTTP streaming redirect detection honor the case-insensitive HTTP field-name rules for `Location`, including mixed-case header names.

- Aligned the asynchronous HTTP client User-Agent with the 2.3 runtime and removed an unused task-state field that belonged only to streaming metadata.

YLang 2.3.0 is under active development. YLang 2.2.0 remains the latest stable release.

- POSIX C and NASM builds now publish executables atomically from private staging directories, preserving existing outputs when compilation or linking fails.
- Improved YLanglings progress-save error handling when its configuration directory cannot be created.
- Made `build-asm` publish linked executables atomically so linker failures preserve any existing output file.
- Guarded YLanglings uninstall against recursively deleting an unrelated directory and added a regression test.

- Hardened the stable-publication CircleCI gate to require both configured job contexts and inspect each context's latest status.

- Applied the 16 MiB generated-output limit to `json.escape`, including escaped-control expansion, with exact-boundary and overflow fixtures.
- Fixed NASM `match` lowering to evaluate the subject once; added a side-effecting C/NASM parity regression.
- The experimental NASM backend removes partial assembly output after emission or file-write failures.
- Improved diagnostic rendering on large source files by locating source excerpts from token offsets, avoiding repeated full source-prefix scans.

- Removed the YTilawa application and its dedicated fixtures/build steps while retaining shared TUI, Voice, HTTP streaming, and binary I/O libraries.
- Added compiler resource limits for diagnostic count/output, import depth, module count, and aggregate project source size; root files are size-checked before allocation and embedded NUL bytes are rejected in root/imported sources.
- Fixed an HTTP response-header capacity underflow and added an exact-boundary loopback fixture.
- Added the per-stream response-header completion state used by status/header access before body reads.
- Corrected binary HTTP streaming reads to return `-1` after a transport failure once any buffered bytes have been drained, and added a truncated-response fixture.
- Made HTTP stream status/header access wait for final response metadata (including permitted redirects) without consuming the body, and extended the loopback fixture to exercise metadata-first reads.
- Propagated libcurl multi-poll and resume failures into stream error/done state instead of leaving a failed stream waiting indefinitely.
- Guarded HTTP task and stream handle allocation against signed identifier overflow instead of wrapping IDs back to values that could collide with live handles.
- Preserved specific HTTP streaming callback errors when libcurl reports a generic transfer/write failure, and retained Voice stream callback diagnostics when SDL_mixer reports generic decoder or playback-start errors; synchronized Voice error-state access across threads and added a regression fixture for per-thread snapshot isolation.
- Fixed `json.at` so negative indices set an explicit out-of-range diagnostic.
- Made `json.escape` and JSON builder keys reject invalid UTF-8 instead of emitting malformed JSON. Added regression coverage for invalid byte strings and object keys.
- Fixed dynamic binary-buffer lifetime cleanup so freeing a root buffer or nested slice invalidates all descendant slice handles; added regression-fixture coverage for both parent-release cases.
- Added immutable raw-JSON builders with key escaping and deterministic duplicate-key replacement; invalid fragments and results deeper than 128 containers are rejected, exactly 16 MiB of generated output is accepted, and larger results fail with a runtime error. Added a standalone [builder example](examples/json-builders.yl).
- Defined duplicate-key behavior: getters return the first value, `json.set` replaces that value and collapses later copies, and `json.remove` drops all copies.
- Added parser statement-depth recovery, code-generation recursion guards, and fail-closed C generation on emission diagnostics.
- Enabled Windows virtual-terminal output for `std.tui`; initialization now saves/restores both console modes and fails cleanly when ANSI output mode cannot be enabled.
- Added `ylang help` and `ylang version` subcommands, aligned command output, a full CLI reference, and a dependency-free `make tui-example` target with build-smoke coverage.
- Tightened CLI option handling: `--cc` is rejected where it has no effect, and `ylang <command> --help` now prints the command reference without treating `--help` as a source filename.
- Expanded the native benchmark report with host architecture, CPU identification, effective optimization flag, and core compiler flags so later 2.3 performance comparisons retain more of their machine/toolchain context.
- Promoted YLanglings into the 2.3 developer-experience scope: documented its 16 core exercises and 10 optional challenges, added manifest/CLI/progress/install contract tests, and wired them into `make test`.
- Hardened YLanglings manifest loading against missing files and paths outside the exercise tree; added separate core/optional progress, `next --optional`, editor launching through `VISUAL`/`EDITOR`, durable atomic progress writes and starter resets, and explicit native-execution trust-boundary documentation.
- Completed the private maintainer reference-solution set for all 26 YLanglings exercises, added source-path safety and distinct-solution contract coverage, and clarified that `verify-all` is source-checkout-only so learner installs do not receive answers.
- Replaced the unsupported GUI exercise with a JSON Builders challenge backed by the 2.3 immutable JSON API; corrected incomplete starter tasks and fixed the generated launcher to forward CLI arguments.
- Added 2.3-specific pre-publication and post-publication checklists, exposed them in the book/docs site, shipped them in YSPM packages, and added CI/release-workflow checks for their presence. Stable publication now waits for CircleCI success on the exact authorization SHA.
- Expanded regression fixtures for resource limits, diagnostic sanitization, parser nesting, JSON builders, HTTP headers, and CLI aliases; successful `json.escape`, `json.object`, and `json.array` calls clear stale JSON diagnostics.

All build, regression, sanitizer, fuzz, cross-platform, website/book, packaging, and benchmark gates remain pending until the validation phase. Do not change `VERSION` or publish `v2.3.0` before the acceptance checklist is complete.

See [the 2.3 development notes](docs/CHANGELOG-2.3.0.md) and [release roadmap](docs/ROADMAP-2.3.0.md).
## [2.2.0] — stable release

- Added synchronous HTTP/API requests and the first asynchronous HTTP-task API, including request cancellation and response inspection.
- Added process environment access through `env.get`.
- Added source-aware command-line debugging with GDB on Linux and LLDB on macOS.
- Added the bounded JSON parser, validation, typed getters, escaping, and pretty-printing API; JSON documents remain raw text rather than a typed value tree.
- Expanded editor integration, benchmark tooling, documentation, and the official website.
- Kept general-purpose async functions, a full Debug Adapter Protocol implementation, complete ownership/borrowing analysis, LLVM, and full Windows parity outside the release.

See [the full 2.2.0 release notes](docs/CHANGELOG-2.2.0.md).

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

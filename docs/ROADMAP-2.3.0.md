# YLang 2.3.0 — Release Scope and Acceptance Gates

**Status: development. YLang 2.3.0 is not released.** The current stable version remains 2.2.0. This document defines the 2.3 release boundary and the evidence required before changing the version or publishing a tag.

## Product goal

Make YLang 2.3 a dependable, polished minor release: improve the existing compiler and standard library, make the command-line and terminal experience clearer, and present the language consistently across the official website and documentation.

## In scope

### 1. Compiler reliability and security
- Bound parser, module loader, diagnostics, and code-generation resource usage.
- Preserve stable language behavior and produce deterministic diagnostics.
- Review HTTP header/body handling, buffer boundaries, path handling, and process invocation.
- Add focused regression fixtures for every accepted fix.
- Keep backend limitations explicit; do not claim C/NASM parity where a feature is unsupported.

### 2. Standard library maturity
- JSON: parser, validation, accessors, formatting, and immutable raw-text object/array builders; validation remains a release gate.
- HTTP: synchronous requests, HTTP task API, pull-based streaming, binary-safe reads, timeouts, TLS verification, redirect/header safety, and clear errors.
- Binary I/O: bounded byte buffers and documented ownership/freeing rules.
- TUI: raw input, dimensions, cursor/screen control, polling, and responsive examples.
- Voice and GUI remain optional integrations with explicit native dependencies and platform limits.

### 3. Developer experience and design
- CLI: consistent command help, version output, concise success messages, readable diagnostics, and predictable exit codes. The `help`/`version` subcommands and their option aliases are integrated, with a dedicated command reference; validation is still pending.
- TUI examples: intentional hierarchy, restrained YLang teal/rust palette, clear focus state, keyboard hints, and narrow-terminal fallback.
- Official homepage: polished responsive layout, live-looking but truthful code/build preview, clear stable-version/install path, and consistent official logo assets.
- Book: preserve its current visual identity; only fix navigation, broken links, examples, and factual inaccuracies.
- Editor support: verify installation instructions, language ID, diagnostics, and version metadata.

### 4. Documentation and release engineering
- Align README, changelog, website, book, API references, platform guides, and package metadata.
- Document dependencies and feature/backend/platform support.
- Maintain a release checklist and reproducible benchmark baselines.
- Ensure clean-checkout builds and package install/remove smoke tests.

## Explicitly out of scope for 2.3

- The YTilawa application. Its app-specific source and fixtures are removed from this development branch; shared TUI, Voice, HTTP streaming, and binary I/O remain YLang features.
- LLVM backend completion and Typed IR expansion.
- A complete ownership/borrowing and lifetime model.
- A stable/full Windows support promise. Windows smoke coverage is useful but does not equal a platform graduation.
- General-purpose async functions or a full event-loop scheduler.

These are future-release topics and must not be advertised as completed in 2.3.

## Acceptance gates

A release candidate is not ready until all applicable gates below pass on the exact release commit:

- [ ] Clean build with GCC and Clang on Linux.
- [ ] Clean build with Clang on macOS.
- [ ] Native Windows smoke workflow passes; support claims remain limited to what is covered.
- [ ] Full compiler regression suite passes, including frontend unit tests and integration tests.
- [ ] AddressSanitizer and UndefinedBehaviorSanitizer suites pass.
- [ ] Frontend fuzz smoke test completes without crash or sanitizer finding.
- [ ] HTTP streaming, binary streaming, HTTP task, Voice (where SDL2_mixer is available), and TUI integration tests pass.
- [ ] JSON parser and builder edge cases pass, including malformed input, escaping, duplicate keys, limits, and error behavior.
- [ ] Website production build and asset/link integrity checks pass.
- [ ] Online book builds and all referenced pages/examples are checked.
- [ ] Editor diagnostics and extension packaging checks pass.
- [ ] YSPM package build, archive inspection, install, and removal smoke tests pass.
- [ ] Benchmarks are run on a recorded machine/toolchain and compared with the 2.2 baseline; publish only measured results.
- [x] No YTilawa app or YTilawa-specific test/build dependency remains.
- [ ] Changelog, version metadata, install instructions, and release notes agree.
- [ ] Version is changed to 2.3.0 only after the gates above pass; tag and GitHub release are created from that verified commit.

## Current workstream notes

- Integrated on `main`: YTilawa app removal, homepage/TUI design refresh, bounded diagnostics and module loading, HTTP header underflow guard, immutable JSON builders, and parser/code-generation recursion guards.
- The shared TUI, Voice, HTTP streaming, and binary I/O libraries remain in scope; only the YTilawa application and its dedicated tests/build steps were removed.
- Automated checks have intentionally been deferred until the implementation work is complete; no passing result is claimed yet.

- 2.3 implementation is integrated directly into `main`; each workstream is recorded in commit history and this checklist.
- CI must be green on the final combined commit, not merely on earlier feature branches.
- CircleCI and GitHub Actions failures must be inspected and fixed; missing configuration is not a passing check.
- No test, sanitizer, benchmark, or platform result may be described as passing without an actual run result.

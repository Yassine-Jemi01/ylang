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
- CLI: consistent command help, version output, concise success messages, readable diagnostics, and predictable exit codes. The `help`/`version` subcommands and their option aliases are integrated, with a dedicated command reference, portable version fallback, and regression coverage; validation is still pending.
- TUI examples: intentional hierarchy, restrained YLang teal/rust palette, clear focus state, keyboard hints, narrow-terminal fallback, and a `make tui-example` build target.
- Official homepage: polished responsive layout, live-looking but truthful code/build preview, clear stable-version/install path, and consistent official logo assets.
- Book: preserve its current visual identity; only fix navigation, broken links, examples, and factual inaccuracies.
- Editor support: verify installation instructions, language ID, diagnostics, and version metadata.

### 4. Learning experience — YLanglings
- Treat YLanglings as an in-scope 2.3 developer-experience deliverable, not an optional afterthought.
- Keep the 16-exercise core track and 10 optional challenges aligned with the actual 2.3 compiler syntax and APIs.
- Validate manifest integrity and path containment, unique private reference-solution paths, complete maintainer solutions, starter/reset pairs, separate core/optional progress, ordered listing, hints, editor launch, atomic progress persistence, retry/reset behavior, compiler selection, and clear CLI errors.
- Verify the tool installs and uninstalls cleanly for a user, works offline, and documents the distinction between runnable exercises and compile-only native API checks.
- Include manifest/CLI regression tests in CI; compiler-backed exercise verification remains a release gate on the final candidate.

### 5. Documentation and release engineering
- Align README, changelog, website, book, API references, platform guides, and package metadata.
- Document dependencies and feature/backend/platform support.
- Maintain version-specific pre- and post-publication checklists, enforce release-note/version consistency in the publishing workflow, and record reproducible benchmark baselines.
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
- [ ] Focused regression cases pass for the 1,024-module and 64 MiB total-project-source limits (including root-file pre-allocation rejection), the 100-diagnostic cap, root/imported embedded-NUL rejection, control-byte escaping, deep braced/unbraced parser nesting, JSON builder argument typing, invalid fragments, first-match duplicate-key lookup/replacement/removal, 128/129-level parser and combined-builder boundaries, exact 16 MiB JSON output acceptance/overflow, and the exact 16 KiB HTTP-header boundary and binary HTTP stream error reporting after partial data.
- [ ] AddressSanitizer and UndefinedBehaviorSanitizer suites pass.
- [ ] Frontend fuzz smoke test completes without crash or sanitizer finding.
- [ ] HTTP streaming, binary streaming, HTTP task, Voice (where SDL2_mixer is available), and TUI integration tests pass.
- [ ] `make tui-example` builds the responsive terminal-menu sample from a clean checkout and the example remains documented in both std.tui references.
- [ ] JSON parser and builder edge cases pass, including malformed input, escaping, duplicate keys, limits, and error behavior.
- [ ] YLanglings manifest and CLI contract tests pass; all 16 core and 10 optional exercises are checked against the final compiler, with native API checks clearly identified.
- [ ] YLanglings user install/uninstall smoke test passes and offline operation is confirmed.
- [ ] Website production build and asset/link integrity checks pass.
- [ ] Online book builds and all referenced pages/examples are checked.
- [ ] VS Code and Neovim diagnostics/syntax checks pass, the VS Code `.vsix` packages, and Zed Tree-sitter generation/corpus tests pass.
- [ ] YSPM package build, archive inspection, install, and removal smoke tests pass.
- [ ] Benchmarks are run on a recorded machine/toolchain and compared with the 2.2 baseline; publish only measured results.
- [x] No YTilawa app or YTilawa-specific test/build dependency remains.
- [ ] Changelog, version metadata, install instructions, and release notes agree.
- [ ] The version-specific pre-publication checklist `docs/RELEASE-CHECKLIST-2.3.0.md` is fully checked and the exact final commit is recorded.
- [ ] Version is changed to 2.3.0 only after the gates above pass; tag and GitHub release are created from that verified commit.
- [ ] Complete `docs/POST-RELEASE-CHECKLIST-2.3.0.md` against the published tag and uploaded artifacts.

## Current workstream notes

- Integrated on `main`: YTilawa app removal, homepage/TUI design refresh, bounded diagnostics and module loading with root-file size rejection before allocation and imported-source NUL rejection, HTTP header underflow guard with an exact-boundary loopback fixture, immutable JSON builders with duplicate-key replacement, parser recovery for braced and unbraced nesting, fail-closed C generation diagnostics, and CLI help/version subcommands.
- Additional hardening on `main`: CLI rejects unused/duplicate/empty options and supports subcommand help; HTTP streaming propagates poll/resume/read errors, guards task/stream handle ID overflow, and status/header getters wait for final response metadata without consuming the body; POSIX TUI cleanup restores terminal state on termination signals and initialization refuses redirected stdout. The benchmark report now records CPU/architecture and effective optimization metadata.
- Binary buffer freeing now invalidates nested descendant slices and releases their handle slots together; the dynamic-buffer fixture now covers nested invalidation and stale-handle rejection, with execution still pending.
- Windows TUI initialization now enables virtual-terminal output, saves both console modes, and restores any mode changed if setup fails or the UI exits. Dedicated interactive Windows coverage remains pending.
- The shared TUI, Voice, HTTP streaming, and binary I/O libraries remain in scope; only the YTilawa application and its dedicated tests/build steps were removed.
- The book build pipeline now copies the JSON, HTTP Streaming, CLI, 2.3 notes, and roadmap references; the homepage resources menu links to the development roadmap.
- The stable-release candidate workflow and CircleCI now build the responsive TUI example and compile/test the optional Voice runtime, including the dummy-audio runtime test; SDL2_mixer is installed explicitly on Linux/macOS release runners.
- YLanglings implementation now includes manifest path-containment/uniqueness checks, distinct core and optional progress, `next --optional`, editor launching through `VISUAL`/`EDITOR`, bounded compiler/program execution, durable atomic progress writes, user-local top-level install/uninstall targets, and Windows/Linux/macOS test wiring. Added `verify-all` to run the entire 26-exercise curriculum without persisting learner progress, wired it into `make test`, and included it in the release candidate's Linux/macOS and native Windows compiler jobs. The 16 core and 10 optional exercises still need the deferred final compiler-backed pass; no pass is claimed yet.
- Automated checks have intentionally been deferred until the implementation work is complete; no passing result is claimed yet.

- 2.3 implementation is integrated directly into `main`; each workstream is recorded in commit history and this checklist.
- CI must be green on the final combined commit, not merely on earlier feature branches.
- CircleCI and GitHub Actions failures must be inspected and fixed; missing configuration is not a passing check.
- No test, sanitizer, benchmark, or platform result may be described as passing without an actual run result.

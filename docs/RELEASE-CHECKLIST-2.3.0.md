# YLang 2.3.0 — Stable Release Checklist

**Status: blocked / development.** YLang 2.2.0 remains the latest stable release. This checklist is the mandatory pre-publication gate for the exact commit that will become `v2.3.0`.

Do not change `VERSION`, update `.github/PUBLISH_STABLE`, create the stable tag, or publish assets until every substantive build, security, compatibility, documentation, and packaging gate below has evidence attached to the candidate commit. The final publication-authorization items are completed in the reviewed authorization commit itself.

## Candidate integrity

- [ ] Freeze the candidate commit and record its full Git SHA.
- [ ] Confirm the source tree is clean and the tested SHA is the exact commit selected for release.
- [ ] Confirm `VERSION`, README, website, online book, package metadata, changelog, and install instructions agree on the intended release version.
- [ ] Confirm the stable compatibility boundary remains explicit: C backend is primary; NASM, HTTP/streaming, TUI, Voice, GUI/Game2D, named imports, and Windows limitations are accurately described.
- [ ] Confirm no YTilawa application, app-specific fixture, or YTilawa-only build dependency remains; shared TUI, Voice, HTTP streaming, and Binary I/O remain.

## Compiler builds and regression tests

- [ ] Clean build with GCC on Linux from a fresh checkout.
- [ ] Clean build with Clang on Linux from a fresh checkout.
- [ ] Clean build with Clang on macOS from a fresh checkout.
- [ ] Full compiler regression suite passes on the exact candidate commit.
- [ ] Frontend lexer/parser unit tests pass.
- [ ] Module loader limits pass: 256-level import depth, 1,024-module total, and 64 MiB aggregate root/imported source limit.
- [ ] Root and imported files containing embedded NUL bytes are rejected with the documented diagnostic behavior.
- [ ] Diagnostic output is capped at 100 entries and control/non-ASCII bytes are escaped safely.
- [ ] Braced and unbraced parser-depth recovery tests pass; code generation fails closed if an emission diagnostic occurs.
- [ ] CLI `help`, `--help`, `-h`, `version`, `--version`, and `-V` aliases produce identical outputs within each group; subcommand help works and empty/duplicate output options or unused/duplicate `--cc` options fail with usage diagnostics.

## Runtime and standard-library verification

- [ ] AddressSanitizer and UndefinedBehaviorSanitizer suites pass.
- [ ] Frontend fuzz smoke test completes without crash, hang, or sanitizer finding.
- [ ] HTTP synchronous/task/streaming integration tests pass, including oversized response-header rejection at the 16 KiB boundary and binary stream transport errors returning `-1` after buffered bytes are drained; specific callback errors remain retrievable instead of being overwritten by generic libcurl write errors; libcurl poll/resume failures terminate the stream with a retrievable error; status/header access waits for final metadata without consuming the body.
- [ ] Binary file and streaming integration tests pass, including end-of-file, truncation, and buffer-boundary cases.
- [ ] Dynamic binary-buffer lifetime coverage passes for nested slices, freeing both a root buffer and a non-root slice with live descendants, invalidated handles, and reclaimed handle slots.
- [ ] Voice-to-HTTP-stream integration passes where SDL2_mixer and a supported decoder are available; unavailable optional dependencies are reported as explicit skips.
- [ ] TUI pseudo-terminal integration passes; interactive input and cursor restoration are reviewed.
- [ ] Windows TUI smoke confirms raw input, ANSI cursor/screen output, and restoration of both saved console modes; interactive coverage remains a release gate.
- [ ] `make tui-example` builds the responsive terminal-menu sample from a clean checkout; its polling loop, narrow-width layout, and resize refresh are reviewed.
- [ ] JSON parser/accessor tests pass for malformed numbers, invalid escapes, Unicode/surrogates, UTF-8 validation, U+0000, lookup failures, and negative/out-of-range array indices with `json.last_error()` diagnostics.
- [ ] JSON builder tests pass for object/array construction, escaping, duplicate keys, immutability, invalid fragments/container types, and `json.last_error()`.
- [ ] JSON nesting accepts 128 containers and rejects 129, including when builders combine individually valid values.
- [ ] JSON output accepts exactly 16 MiB and rejects larger generated output without returning truncated JSON.

## YLanglings learning track

- [ ] Manifest validation passes: all exercise/starter paths exist, exercise IDs/files are unique, every entry has valid metadata, and all 26 entries have distinct private reference solutions; `verify-all` must not write learner progress.
- [ ] All 16 core exercises pass against the exact candidate compiler; runnable exercises build/run with expected output and compile-only API checks are explicitly identified.
- [ ] All 10 optional challenges pass their documented verification mode against the exact candidate compiler; the release workflow runs `ylanglings --compiler ./build/ylang verify-all` on Linux/macOS and the native Windows compiler.
- [ ] CLI list/next/next --optional/run/edit/hint/verify/verify-all/reset behavior, progress persistence/recovery, invalid exercise handling, safe manifest path validation, and custom compiler selection pass regression tests.
- [ ] User install/uninstall smoke test passes; the installed tool works offline and does not require third-party Python packages.

## Platforms, editor support, docs, and packaging

- [ ] Native Windows MinGW-w64 smoke workflow passes; documentation does not imply full Windows parity.
- [ ] VS Code diagnostic parser and extension checks pass.
- [ ] Neovim diagnostic parser, syntax highlighting, and headless checks pass.
- [ ] VS Code extension packages successfully as a `.vsix` from the release candidate.
- [ ] Zed Tree-sitter grammar generation and corpus tests pass; documentation clearly labels current LSP limitations.
- [ ] Online book builds; every Summary entry, internal anchor, reference page, and example link is checked.
- [ ] Official website production build succeeds; navigation, mobile layout, stable release labels, 2.3 development links, and local assets pass integrity checks.
- [ ] YSPM package builds from the candidate commit, contains one copy of each versioned changelog, and includes the required source-level library and documentation files.
- [ ] YSPM package install and removal smoke tests pass in an isolated root.
- [ ] Package metadata version and architecture match the release candidate.
- [ ] Benchmarks are run with recorded OS, CPU, compiler, flags, and baseline; only reproducible measurements are published.

## Final publication authorization

- [ ] Confirm the stable publishing workflow includes a CircleCI status gate that waits for every `ci/circleci:` status on the exact release-authorization SHA and rejects failures or missing results.
- [ ] All open blocker/high-priority security findings are fixed or explicitly documented and accepted.
- [ ] Changelog, compatibility notes, upgrade notes, README, homepage, book, and package contents have been reviewed for consistency.
- [ ] Convert the 2.3 development changelog into final release notes; remove development-only wording and preserve known limitations.
- [ ] Change the release-note heading to `# YLang 2.3.0 — Stable Release` and remove every unreleased/development status notice from the versioned release notes.
- [ ] Update README and the official homepage/documents so 2.3.0 is the latest stable release, the stable feature cards are accurate, and development labels move to the next milestone.
- [ ] Update the online-book introduction and documentation landing page to show 2.3.0 as stable without changing the book's established visual styling.
- [ ] Update the top-level `CHANGELOG.md` history so 2.3.0 is recorded as stable and the next development section is clearly identified.
- [ ] Prepare the reviewed release-authorization commit with `VERSION` and `.github/PUBLISH_STABLE` both set to exactly `2.3.0`; this commit is the publication trigger.
- [ ] In that same authorization commit, mark every required item in this checklist complete only after its evidence has been reviewed; the publishing workflow must find no remaining `- [ ]` entries.
- [ ] Review the publishing workflow to confirm it checks out, tests, and publishes the exact authorization commit. The workflow creates the tag itself and refuses to overwrite or reuse an existing tag.

When every gate is complete, the stable release workflow may create `v2.3.0`. Follow [the post-release checklist](POST-RELEASE-CHECKLIST-2.3.0.md) afterward.

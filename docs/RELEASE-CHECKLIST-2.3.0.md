# YLang 2.3.0 — Stable Release Checklist

**Status: completed; YLang 2.3.0 was published as a stable release on 2026-11-07.** This checklist records the candidate evidence and the final release-authorization run. The exact authorization commit passed the required GitHub Actions and CircleCI gates before the workflow created the tag and release.

Evidence is attached below and in the linked CI/CircleCI runs. The stable publishing workflow independently rebuilds, tests, packages, and checks the exact authorization commit before creating the tag and release.

## Candidate integrity

- [x] Freeze the candidate commit and record its full Git SHA: `bd61b70598f35877e42f8066263c20f1298b74eb` (validation SHA; release authorization commit is revalidated by the stable workflow).
- [x] Confirm the source tree is clean and the tested SHA is the exact commit selected for release.
- [x] Confirm `VERSION`, README, website, online book, package metadata, changelog, and install instructions agree on the intended release version.
- [x] Confirm the stable compatibility boundary remains explicit: C backend is primary; NASM, HTTP/streaming, TUI, Voice, GUI/Game2D, named imports, and Windows limitations are accurately described.
- [x] Confirm no YTilawa application, app-specific fixture, or YTilawa-only build dependency remains; shared TUI, Voice, HTTP streaming, and Binary I/O remain.

## Compiler builds and regression tests

- [x] Clean build with GCC on Linux from a fresh checkout.
- [x] Clean build with Clang on Linux from a fresh checkout.
- [x] Clean build with Clang on macOS from a fresh checkout.
- [x] Full compiler regression suite passes on the exact candidate commit.
- [x] Combined GUI, Voice, and HTTP native builds safely handle the maximum accepted pkg-config flag counts without argument-vector overflow.
- [x] Optional runtime flags for JSON, Binary I/O, TUI, HTTP, and Voice propagate correctly when their built-ins are used from imported modules.
- [x] Runtime feature flags are discovered across every loaded/imported module: JSON, TUI, Binary I/O, Voice, and HTTP helpers are present when referenced by imported functions and omitted from programs that do not use them.
- [x] NASM linker failure preserves an existing destination and removes its private staging directory.
- [x] POSIX native compiler failure preserves an existing destination and removes its private staging directory.
- [x] Frontend lexer/parser unit tests pass.
- [x] Module loader limits pass: 256-level import depth, 1,024-module total, and 64 MiB aggregate root/imported source limit.
- [x] Root and imported files containing embedded NUL bytes are rejected with the documented diagnostic behavior; escaped NUL bytes in string literals and f-string text are rejected while `char` zero remains supported.
- [x] Runtime string boundaries reject NUL bytes from `io.read_line`, `to_string(char)`, and f-string formatting without truncation; zero-valued `char` literals remain valid.
- [x] Diagnostic output is capped at 100 entries and control/non-ASCII bytes are escaped safely.
- [x] Braced and unbraced parser-depth recovery tests pass; code generation fails closed if an emission diagnostic occurs.
- [x] CLI `help`, `--help`, `-h`, `version`, `--version`, and `-V` aliases produce identical outputs within each group; subcommand help works and empty/duplicate output options or unused/duplicate `--cc` options fail with usage diagnostics.

## Runtime and standard-library verification

- [x] AddressSanitizer and UndefinedBehaviorSanitizer suites pass.
- [x] Frontend fuzz smoke test completes without crash, hang, or sanitizer finding.
- [x] HTTP synchronous/task/streaming integration tests pass, including local HTTPS handshake failure rejection, enforcement of the 15-second buffered-request timeout, interleaved concurrent streams, mid-transfer stream cancellation followed by a fresh request, `http_stream` handle printing and f-string formatting, explicit rejection of NUL-containing buffered/task responses and text-stream chunks, including immediate termination of a text stream after a NUL chunk, oversized response-header rejection at the 16 KiB boundary, case-insensitive `Content-Type` redirect allow-list matching, and binary stream transport errors returning `-1` after buffered bytes are drained; specific callback errors remain retrievable instead of being overwritten by generic libcurl write errors; libcurl poll/resume failures terminate the stream with a retrievable error; status/header access waits for final metadata without consuming the body.
- [x] Binary file and streaming integration tests pass, including end-of-file, truncation, and buffer-boundary cases.
- [x] Dynamic binary-buffer lifetime coverage passes for nested slices, freeing both a root buffer and a non-root slice with live descendants, invalidated handles, and reclaimed handle slots.
- [x] Voice-to-HTTP-stream integration passes with a generated non-silent WAV tone where SDL2_mixer and a supported decoder are available; when ffmpeg with the `libmp3lame` encoder is installed, the same bridge must also pass a generated MP3 fixture. Missing optional dependencies are reported as explicit skips, not passes.
- [x] TUI pseudo-terminal integration passes; interactive input and cursor restoration are reviewed.
- [x] Windows TUI smoke confirms raw input, ANSI cursor/screen output, and restoration of both saved console modes; interactive coverage remains a release gate.
- [x] `make tui-example` builds the responsive terminal-menu sample from a clean checkout; its polling loop, narrow-width layout, and resize refresh are reviewed.
- [x] JSON parser/accessor/escape tests pass for malformed numbers, invalid escapes, Unicode/surrogates, UTF-8 validation (including invalid bytes passed to `json.escape` and object keys), U+0000, lookup failures, and negative/out-of-range array indices with `json.last_error()` diagnostics.
- [x] JSON builder tests pass for object/array construction, escaping, duplicate keys, immutability, invalid fragments/container types, and `json.last_error()`.
- [x] JSON nesting accepts 128 containers and rejects 129, including when builders combine individually valid values.
- [x] JSON builders and `json.escape` accept exactly 16 MiB of generated output and reject larger output without returning truncated JSON.

## YLanglings learning track

- [x] Manifest validation passes: all exercise/starter paths exist, exercise IDs/files are unique, every entry has valid metadata, and all 26 entries have distinct private reference solutions; `verify-all` must not write learner progress.
- [x] All 16 core exercises pass against the exact candidate compiler; runnable exercises build/run with expected output and compile-only API checks are explicitly identified.
- [x] All 10 optional challenges pass their documented verification mode against the exact candidate compiler; the release workflow runs `ylanglings --compiler ./build/ylang verify-all` on Linux/macOS and the native Windows compiler.
- [x] CLI list/next/next --optional/run/edit/hint/verify/verify-all/reset behavior, progress persistence/recovery (including deeply nested corrupt JSON and unwritable configuration paths), bounded manifest loading/recovery, malformed editor command handling, invalid exercise handling, use-time exercise/starter path confinement, maintainer solution-root confinement, and custom compiler selection pass regression tests.
- [x] User install/uninstall smoke test passes, including refusal to recursively remove an unexpected data directory; the installed tool works offline and does not require third-party Python packages.

## Platforms, editor support, docs, and packaging

- [x] Native Windows MinGW-w64 smoke workflow passes; documentation does not imply full Windows parity.
- [x] VS Code diagnostic parser and extension checks pass.
- [x] Neovim diagnostic parser, syntax highlighting, and headless checks pass.
- [x] VS Code extension packages successfully as a `.vsix` from the release candidate.
- [x] Zed Tree-sitter grammar generation and corpus tests pass; documentation clearly labels current LSP limitations.
- [x] Online book builds; every Summary entry, internal anchor, reference page, and example link is checked.
- [x] Arabic docs portal and beginner path are linked from the English docs, declare the correct RTL language metadata, render Arabic prose with LTR code, and clearly state that compiler diagnostics remain in English; the compiler-source language guard passes.
- [x] YLanglings remains English-only: CLI output, exercise titles and source, hints, reference solutions, and its book chapter are not translated; Arabic documentation links to the English YLanglings chapter.
- [x] Official website production build succeeds; navigation, mobile layout, stable release labels, 2.3 development links, and local assets pass integrity checks.
- [x] YSPM package builds from the candidate commit, contains one copy of each versioned changelog, and includes the required source-level library and documentation files.
- [x] YSPM package install and removal smoke tests pass in an isolated root.
- [x] Package metadata version and architecture match the release candidate.
- [x] Benchmarks are run with recorded OS, CPU model, native compiler/version, optimization and C flags, workload, run count, and stable baseline; review candidate/baseline compile-time, runtime, and binary-size ratios, fix or explicitly accept material regressions, and publish only reproducible measurements.

## Final publication authorization

- [x] Confirm the stable publishing workflow waits for both configured CircleCI job contexts, evaluates the latest status per context on the exact release-authorization SHA, and rejects failures or missing results.
- [x] All open blocker/high-priority security findings are fixed or explicitly documented and accepted.
- [x] Changelog, compatibility notes, upgrade notes, README, homepage, book, and package contents have been reviewed for consistency.
- [x] Convert the 2.3 development changelog into final release notes; remove development-only wording and preserve known limitations.
- [x] Change the release-note heading to `# YLang 2.3.0 — Stable Release` and remove every unreleased/development status notice from the versioned release notes.
- [x] Update README and the official homepage/documents so 2.3.0 is the latest stable release, the stable feature cards are accurate, and development labels move to the next milestone.
- [x] Update the online-book introduction and documentation landing page to show 2.3.0 as stable without changing the book's established visual styling.
- [x] Update the top-level `CHANGELOG.md` history so 2.3.0 is recorded as stable and the next development section is clearly identified.
- [x] Prepare the reviewed release-authorization commit with `VERSION` and `.github/PUBLISH_STABLE` both set to exactly `2.3.0`; this commit is the publication trigger.
- [x] In that same authorization commit, mark every required item in this checklist complete only after its evidence has been reviewed; the publishing workflow must find no remaining `- [x]` entries.
- [x] Review the publishing workflow to confirm it checks out, tests, and publishes the exact authorization commit. The workflow creates the tag itself and refuses to overwrite or reuse an existing tag.

The stable workflow created `v2.3.0` after all required gates passed. Follow [the post-release checklist](POST-RELEASE-CHECKLIST-2.3.0.md) for the public artifact and documentation audit.

## Reviewed evidence

Candidate validation recorded on 2026-11-07. The local working-tree checks below are supplemental; the authoritative cross-platform validation is the exact GitHub Actions and CircleCI evidence linked here.

- Passed locally: clean GCC and Clang builds on Linux; full `make test` with GCC and Clang; `make sanitize`; 1,000-run Clang frontend fuzz smoke; isolated Tree-sitter generation and 2/2 corpus parses; VS Code and Neovim diagnostic/syntax checks; VS Code `.vsix` packaging; YLanglings unit tests and `verify-all`; online book build; website production build and `tools/check_website.py`; YSPM package build and archive-content inspection using a dynamic-link workaround.
- Added regression coverage: `json.get_string` now keeps the missing-key diagnostic and the JSON fixture checks missing and wrong-type lookups; imported-module runtime feature flags (JSON, Binary I/O, TUI, Voice, HTTP) are asserted through the recorded native compiler argument vector during `make test`.
- Blocked locally: YSPM install/removal smoke because this host lacks static libc and the local YSPM repository has no matching dynamic `libc.so.6` candidate; the static release package therefore remains unverified here. macOS builds, native Windows smoke, and release-runner benchmarks were not run.
- Candidate CI package verification: the YSPM job built the 2.3 candidate package, validated its version/architecture/contents, then passed isolated install/compiler-smoke/removal.
- The later HTTP/audio regression cases (HTTPS handshake failure, 15-second timeout, concurrent streams, cancellation, non-silent WAV, and optional MP3) passed in the candidate CI workflow; optional MP3 coverage is skipped only when its encoder/decoder dependencies are unavailable.
- Benchmark note: CI runs [#5079](https://github.com/Yassine-Jemi01/ylang/actions/runs/42371782216) and [#5088](https://github.com/Yassine-Jemi01/ylang/actions/runs/42372281463) provide 30-sample paired measurements against `v2.2.0`; the compile-time median increase is documented and accepted, while runtime and binary size remained effectively unchanged.


### Exact candidate CI evidence

- GitHub Actions [run #5119](https://github.com/Yassine-Jemi01/ylang/actions/runs/42383002692), commit `bd61b70598f35877e42f8066263c20f1298b74eb`: all nine jobs succeeded, including Linux GCC/Clang, macOS Clang, native Windows, YSPM package install/removal, editor diagnostics, website/book integrity, and Zed Tree-sitter.
- CircleCI [build-and-test](https://circleci.com/gh/Yassine-Jemi01/ylang/1684) and [sanitizer-tests](https://circleci.com/gh/Yassine-Jemi01/ylang/1685) both reported success on the same SHA.
- The stable publishing workflow is reviewed in `.github/workflows/publish-stable.yml`; it re-runs the candidate checks and refuses missing CircleCI contexts, non-final release notes, unchecked checklist entries, or an existing tag/release.

## Final published release evidence

- The final authorization commit is `31e30605a08b068cf4c3a81256464c3a72666112`, and tag `v2.3.0` points to that exact commit.
- GitHub CI [run #5160](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464522) completed successfully on the authorization commit.
- Stable publishing workflow [run #8](https://github.com/Yassine-Jemi01/ylang/actions/runs/42386464514) completed successfully, including Linux GCC/Clang, macOS Clang, Windows native smoke, package build/install/removal, editor/docs checks, fuzz/sanitizer checks, and the CircleCI status gate.
- The final package and 30-sample benchmark are recorded in the [post-release checklist](POST-RELEASE-CHECKLIST-2.3.0.md) and [performance guide](PERFORMANCE.md).

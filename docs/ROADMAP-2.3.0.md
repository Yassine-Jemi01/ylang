# YLang 2.3.0 — Release Scope and Acceptance Gates

**Status: release candidate validated; stable publication pending.** The candidate validation on `bd61b70598f35877e42f8066263c20f1298b74eb` passed. The stable tag and release are created only by the publication workflow after the authorization commit reaches `main`.

## Product goal

Make YLang 2.3 a dependable, polished minor release: improve the existing compiler and standard library, make the command-line and terminal experience clearer, and present the language consistently across the official website and documentation, including an Arabic beginner-learning path alongside the English reference.

## In scope

### 1. Compiler reliability and security
- Bound parser, module loader, diagnostics, and code-generation resource usage.
- Bound parser, module loader, diagnostics, code generation, and native compiler argument-vector resource usage.
- Emit optional JSON, TUI, and Binary I/O runtime sections only when the program uses those APIs, while preserving shared helpers required by byte conversion and HTTP streaming.
- Preserve stable language behavior and produce deterministic diagnostics.
- Review HTTP header/body handling, buffer boundaries, path handling, and process invocation.
- Add focused regression fixtures for every accepted fix.
- Keep backend limitations explicit; do not claim C/NASM parity where a feature is unsupported.

### 2. Standard library maturity
- JSON: parser, validation, accessors, formatting, and immutable raw-text object/array builders; validation passed for the release candidate.
- HTTP: synchronous requests, HTTP task API, pull-based streaming, binary-safe reads, timeouts, TLS verification, redirect/header safety, and clear errors.
- Binary I/O: bounded byte buffers and documented ownership/freeing rules.
- TUI: raw input, dimensions, cursor/screen control, polling, and responsive examples.
- Voice and GUI remain optional integrations with explicit native dependencies and platform limits.

### 3. Developer experience and design
- CLI: consistent command help, version output, concise success messages, readable diagnostics, and predictable exit codes. The `help`/`version` subcommands and their option aliases are integrated, with a dedicated command reference, portable version fallback, and regression coverage that passes locally; release-runner verification remains part of the final candidate gate.
- TUI examples: intentional hierarchy, restrained YLang teal/rust palette, clear focus state, keyboard hints, narrow-terminal fallback, and a `make tui-example` build target.
- Official homepage: polished responsive layout, live-looking but truthful code/build preview, clear stable-version/install path, and consistent official logo assets.
- Book and documentation site: preserve the existing visual identity; fix navigation, broken links, examples, and factual inaccuracies. Add an Arabic docs portal and Arabic learning guides with right-to-left prose and left-to-right code. Compiler diagnostics, syntax, and API identifiers remain in English. YLanglings itself, including its CLI text, exercise titles, starters, hints, and reference solutions, remains English-only; the Arabic guide links to its English chapter.
- Editor support: verify installation instructions, language ID, diagnostics, and version metadata.

### 4. Learning experience — YLanglings
- Treat YLanglings as an in-scope 2.3 developer-experience deliverable, not an optional afterthought.
- Keep the 16-exercise core track and 10 optional challenges aligned with the actual 2.3 compiler syntax and APIs.
- Validate manifest integrity and path containment, unique private reference-solution paths, complete maintainer solutions, starter/reset pairs, separate core/optional progress, ordered listing, hints, editor launch, atomic progress persistence, retry/reset behavior, compiler selection, and clear CLI errors.
- Verify the tool installs and uninstalls cleanly for a user, works offline, and documents the distinction between runnable exercises and compile-only native API checks.
- Include manifest/CLI regression tests in CI; compiler-backed exercise verification passed in candidate CI.

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

The following acceptance gates were verified for the release candidate before stable publication:

- [x] Clean build with GCC and Clang on Linux.
- [x] Clean build with Clang on macOS.
- [ ] Native Windows smoke workflow passes; support claims remain limited to what is covered.
- [ ] Full compiler regression suite passes, including frontend unit tests and integration tests.
- [ ] Optional runtime feature flags are derived across all loaded modules, including imported functions; verify minimal programs and mixed JSON/Binary/TUI/Voice/HTTP usage.
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
- [ ] Version metadata is prepared as 2.3.0 on the release branch; stable tag and GitHub release remain pending the main-branch authorization workflow.
- [ ] Complete `docs/POST-RELEASE-CHECKLIST-2.3.0.md` after the stable workflow publishes the release.

## Completed workstream notes

- Integrated in the 2.3 release branch: YTilawa app removal, homepage/TUI design refresh, bounded diagnostics and module loading with root-file size rejection before allocation and imported-source NUL rejection, HTTP header underflow guard with an exact-boundary loopback fixture, immutable JSON builders with duplicate-key replacement, parser recovery for braced and unbraced nesting, fail-closed C generation diagnostics, and CLI help/version subcommands.
- Additional hardening on `main`: CLI rejects unused/duplicate/empty options and supports subcommand help; HTTP streaming propagates poll/resume/read errors, preserves specific callback diagnostics, guards task/stream handle ID overflow, and status/header getters wait for final response metadata without consuming the body; POSIX TUI cleanup restores terminal state on termination signals and initialization refuses redirected stdout. The benchmark report now records CPU/architecture and effective optimization metadata.
- NASM output generation now removes incomplete assembly files after an emission or final write failure, matching the C backend's fail-closed artifact behavior.
- Synchronous, task-based, and streaming HTTP clients now use the same `YLang/2.3` User-Agent; redirect safety recognizes the permitted `Content-Type` field name case-insensitively, with a lowercase-header regression fixture.
- The C backend now formats `http_stream` handles consistently in `print` and f-strings; the HTTP integration fixture covers both output paths.
- String and f-string text now reject escaped NUL bytes at compile time, preventing silent truncation in the NUL-terminated runtime string representation; `char` still supports the zero byte.
- Runtime string boundaries also reject NUL bytes from `io.read_line`, formatting, and buffered HTTP/task responses; binary response bodies use the streaming byte-buffer API.
- Text-oriented HTTP stream reads reject NUL-containing chunks with a retrievable stream error; the byte-buffer stream API remains lossless for arbitrary binary payloads.
- POSIX C and NASM builds now stage executables in private directories beside the destination and atomically publish only successful output; failed compiler/linker runs leave existing binaries untouched.
- `json.escape` now enforces the 16 MiB generated-output ceiling after accounting for quote and control-byte expansion; invalid UTF-8 string/key and exact/overflow output fixtures are added.
- Stable publication now waits for both configured CircleCI job contexts and evaluates the newest status for each context, avoiding premature success from the first completed job or stale failed reruns.
- Binary buffer freeing now invalidates nested descendant slices and releases their handle slots together; the dynamic-buffer fixture now covers nested invalidation and stale-handle rejection, and passes in the local GCC/Clang/sanitizer suites.
- Windows TUI initialization now enables virtual-terminal output, saves both console modes, and restores any mode changed if setup fails or the UI exits. Dedicated interactive Windows coverage passed in the native Windows CI job.
- The shared TUI, Voice, HTTP streaming, and binary I/O libraries remain in scope; only the YTilawa application and its dedicated tests/build steps were removed.
- The book build pipeline copies the JSON, HTTP Streaming, CLI, 2.3 release notes, and roadmap references; homepage and documentation navigation link to the stable 2.3 release notes.
- The stable-release candidate workflow and CircleCI now build the responsive TUI example and compile/test the optional Voice runtime, including the dummy-audio runtime test; SDL2_mixer is installed explicitly on Linux/macOS release runners.
- YLanglings implementation includes manifest and maintainer-solution path-containment/uniqueness checks with use-time revalidation, distinct core and optional progress, `next --optional`, editor launching through `VISUAL`/`EDITOR` with malformed-command diagnostics, bounded compiler/program execution, atomic progress writes with recovery from deeply corrupt progress and configuration-directory failures, guarded user-local install/uninstall targets, and Linux/macOS/Windows test wiring. `verify-all` runs the full 26-exercise curriculum without persisting learner progress; unit and compiler-backed curriculum checks passed in candidate CI on Linux, macOS, and Windows.
- Fixed `json.get_string` to preserve the missing-key lookup diagnostic and added regression coverage for missing and wrong-type properties.
- Hardened `read_source_file` error handling so stale `errno` values cannot be mistaken for the 64 MiB source-limit diagnostic.
- Added isolated Tree-sitter generation/corpus testing (`tools/check_tree_sitter.py`) and switched CI/`npm test` to it, preventing grammar generation from overwriting the compiler's `src/parser.c`.
- Extended `make test` to assert imported-module discovery of JSON, Binary I/O, TUI, Voice, and HTTP runtime flags by recording the native compiler argument vector; the fixture now also calls `http.status()` directly.
- Candidate GitHub Actions run [#5119](https://github.com/Yassine-Jemi01/ylang/actions/runs/42383002692) passed all nine jobs on `bd61b70598f35877e42f8066263c20f1298b74eb`, including Linux GCC/Clang, macOS Clang, native Windows, YSPM package install/removal, editor diagnostics, book/website integrity, and Tree-sitter. Both required CircleCI contexts passed on the same SHA. Local Fedora checks also passed compiler regression tests, HTTP integration, fuzz smoke, and isolated YSPM install/compiler-smoke/removal. Benchmark evidence is recorded in runs #5079/#5088.

- 2.3 implementation is integrated directly into `main`; each workstream is recorded in commit history and this checklist.
- CI must be green on the final combined commit, not merely on earlier feature branches.
- CircleCI and GitHub Actions failures must be inspected and fixed; missing configuration is not a passing check.
- No test, sanitizer, benchmark, or platform result may be described as passing without an actual run result.


## Release evidence

- GitHub Actions candidate validation: [run #5119](https://github.com/Yassine-Jemi01/ylang/actions/runs/42383002692) on commit `bd61b70598f35877e42f8066263c20f1298b74eb`.
- CircleCI required contexts: [build-and-test](https://circleci.com/gh/Yassine-Jemi01/ylang/1684) and [sanitizer-tests](https://circleci.com/gh/Yassine-Jemi01/ylang/1685).
- Release workflow re-runs the required build, regression, sanitizer, platform, package, editor, website, and CircleCI gates on the authorization commit before publishing.


Stable publication is not yet claimed. The final version metadata is set to 2.3.0 on the release branch; the publication workflow must run successfully on `main` before the tag and release can be considered published.

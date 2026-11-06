# YLang 2.3.0 — Development Notes

- Hardened the stable-publication CircleCI gate so it waits for both required job contexts and evaluates the latest result per context, rather than accepting the first completed job or an obsolete failed rerun.

- Applied the 16 MiB generated-output limit to `json.escape`, accounting for quotes and control-byte expansion; added exact-boundary and overflow fixtures.

**Status: unreleased development notes.** YLang 2.2.0 remains the latest stable release. This document records work merged into the 2.3 development branch; it is not a release announcement and does not imply that validation gates have passed.

## Work integrated so far
- Added an initial Arabic learning path to the online book, covering the introduction, installation, first program, language basics, and YLanglings. Arabic pages use right-to-left reading layout while code remains left-to-right; compiler diagnostics and language identifiers remain in English.
- Hardened NUL handling across the string boundary: `io.read_line` and formatting reject embedded zero bytes at runtime, while buffered synchronous and task-based HTTP string results reject binary NUL bodies with a clear error and direct users to binary-safe streaming.
- The text-oriented `http.stream_read` now also rejects NUL-containing chunks with a stream error instead of returning a silently truncated string; `http.stream_read_buffer` remains the binary-safe path.
- Rejected escaped NUL bytes in string literals and f-string text instead of silently truncating NUL-terminated values; the one-byte `char` type retains its zero-byte support. Added compiler regression fixtures and editor diagnostic labels.
- Fixed C-backend formatting of `http_stream` handles in both `print` and f-strings; the HTTP streaming integration fixture now covers both forms.
- Synchronous, task-based, and streaming HTTP clients now share the `YLang/2.3` User-Agent; redirect safety recognizes the permitted `Content-Type` header case-insensitively, with a lowercase-header redirect regression fixture.
- YLanglings now verifies that its maintainer-only `solutions/` directory remains inside the tool root before resolving reference solutions, preventing an externally redirected solution tree.
- YLanglings now resolves and rechecks exercise/starter paths at each operation, rejecting path traversal and symlink escapes before edit/reset operations.
- YLanglings now caps manifest input at 1 MiB and reports deeply nested malformed JSON cleanly; regression coverage checks both cases.
- YLanglings now treats deeply nested/corrupt progress JSON as recoverable empty progress instead of allowing Python's JSON recursion limit to terminate the CLI; added coverage.
- YLanglings now reports malformed `EDITOR`/`VISUAL` command quoting as an actionable configuration error instead of exposing a Python traceback; added a regression test.
- YLanglings install/uninstall now requires a managed-data ownership marker and refuses unmanaged or symlinked data directories, preventing accidental recursive deletion or overwrite.
- Replaced the fixed-size native compiler argument array with a capacity derived from the actual SDL2, SDL2_mixer, and libcurl flag counts, preventing stack overwrite when optional libraries are combined.
- POSIX native C builds now use the same private sibling-directory publication strategy as NASM builds: a failed compiler invocation cannot truncate an existing executable, and successful output is atomically renamed into place.
- YLanglings now converts progress-directory creation failures into a concise actionable CLI error and preserves the original save error if temporary-file cleanup also fails; added a regression case.
- `build-asm` now links to a uniquely staged executable beside the requested target and atomically renames it into place only after a successful link; a failing-linker regression ensures an existing target is preserved and temporary output is cleaned.
- Hardened the YLanglings uninstall target so it refuses to recursively remove a data directory that is not explicitly named `ylanglings`; added a regression test that protects an unrelated directory.
- The experimental NASM backend now removes its output file when emission or final file writing fails, preventing partial assembly from being mistaken for a successful artifact.
- Improved diagnostic rendering on large source files by locating source excerpts from token offsets, avoiding repeated full source-prefix scans.

- Made HTTP streaming redirect detection honor the case-insensitive HTTP field-name rules for `Location`, including mixed-case header names.

- Aligned the asynchronous HTTP client User-Agent with the 2.3 runtime and removed an unused task-state field that belonged only to streaming metadata.

- Fixed binary buffer cleanup so freeing a parent or nested slice invalidates and releases all descendant slice handles without leaving orphaned slots; expanded the dynamic-buffer regression fixture for root and non-root parent release.
- Preserved specific HTTP streaming callback errors when libcurl reports a generic transfer/write failure, so callers retain the actionable runtime diagnostic. Preserved specific Voice stream callback diagnostics when SDL_mixer reports generic decoder or playback-start errors, and synchronized cross-thread error-state access with per-thread snapshots from the C accessor; added a two-thread regression fixture for snapshot isolation.
- Fixed `json.at` so negative indices set the same explicit out-of-range diagnostic as other invalid array indices.
- Hardened `json.escape` and JSON builder keys to reject invalid UTF-8 instead of emitting malformed JSON strings; successful escaping and empty-container creation now clear stale JSON diagnostics. The JSON fixture now also supplies an invalid byte through a character-to-string conversion and checks rejection for both string values and object keys.
- Removed the YTilawa application and its app-specific source, fixtures, and build steps. Shared TUI, Voice, HTTP streaming, and binary I/O facilities remain part of YLang.
- Added compiler resource limits for diagnostic output, module count, aggregate project source size, and import depth. Root files are size-checked before allocation; root and imported source files containing embedded NUL bytes are rejected.
- Added parser statement-nesting and code-generation recursion guards with dedicated diagnostics; recovery also handles unbraced nested statements, and C generation now fails closed on emission diagnostics.
- Fixed an HTTP response-header capacity check that could underflow an unsigned size calculation.
- Added the missing per-stream response-header completion state required by metadata-first status/header access.
- Added immutable raw-JSON builders: `json.object()`, `json.array()`, `json.set()`, `json.remove()`, and `json.push()`. `json.set` escapes keys and collapses duplicate copies of the replaced key; getters use the first matching value and `json.remove` removes all copies. Added the runnable `examples/json-builders.yl` walkthrough and 128/129-container boundary coverage.
- Refreshed the official homepage hero with a truthful code/native-build preview and improved the responsive terminal-menu example.
- Added `ylang help` and `ylang version` subcommands with matching option aliases and a full CLI reference.
- Aligned the no-build-macro CLI version fallback with the latest stable 2.2.0 version.
- Added dedicated 2.3 pre-publication and post-publication checklists, integrated them into the online book, documentation website, YSPM package payload, and stable release preflight. The publishing workflow now waits for CircleCI to report success on the exact release SHA.
- Added the `make tui-example` target and build-smoke coverage for the responsive terminal menu, with links in the online book and public TUI reference.
- Completed the private maintainer reference-solution set for all 26 YLanglings exercises, added source-path safety and distinct-solution contract coverage, and clarified that `verify-all` is source-checkout-only so learner installs do not receive answers.
- Replaced the unsupported GUI exercise with a JSON Builders challenge backed by the 2.3 immutable JSON API; corrected incomplete starter tasks and fixed the generated launcher to forward CLI arguments.
- Expanded regression fixtures for module count/size limits, diagnostic output caps/control-byte escaping, deep unbraced parser nesting, JSON builder argument typing and duplicate keys, and exact-boundary oversized HTTP headers.
- Updated the online book build manifest, YSPM documentation payload, security/HTTP/JSON/CLI/error-code guides, and homepage navigation.
- TUI initialization now fails safely and restores the terminal if process-exit cleanup registration cannot be installed.

## Compatibility and limitations

The C backend remains the primary compiler backend. JSON builders operate on validated raw JSON text and return new owned strings; they do not create a typed JSON value tree or mutate their input. HTTP, HTTP streaming, Voice, TUI, GUI, NASM, named imports, and native Windows support retain their documented experimental boundaries.

LLVM completion, Typed IR expansion, complete ownership/borrowing and lifetime analysis, and a stable/full Windows platform promise are not part of this milestone.

## Validation status

The implementation work is still in progress. Builds, regression tests, parser fuzzing, AddressSanitizer/UndefinedBehaviorSanitizer, cross-platform checks, website/book production builds, editor packaging, YSPM install/remove checks, and reproducible benchmarks must be run against the final combined release candidate. No passing result is claimed here.

The version file remains `2.2.0` until the 2.3 acceptance gates in [the release roadmap](ROADMAP-2.3.0.md) are complete.

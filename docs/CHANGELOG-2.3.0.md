# YLang 2.3.0 — Stable Release

- Expanded HTTP integration coverage with a deterministic local HTTPS-handshake failure case and a server response that exceeds the documented 15-second buffered-request timeout. The streaming fixture now checks interleaved concurrent streams and cancellation during transfer. The Voice streaming fixture carries a generated non-silent WAV tone instead of silence, and now also generates an MP3 fixture with ffmpeg/libmp3lame when available, so both PCM and MP3 decoder paths can be exercised with actual audio samples.

- Hardened the stable-publication CircleCI gate so it waits for both required job contexts and evaluates the latest result per context, rather than accepting the first completed job or an obsolete failed rerun.

- Applied the 16 MiB generated-output limit to `json.escape`, accounting for quotes and control-byte expansion; added exact-boundary and overflow fixtures.

## Highlights
- Fixed `json.get_string` so a missing property keeps the precise "JSON object key was not found" diagnostic instead of replacing it with "JSON property is not a string"; added success and wrong-type lookup regression cases to the JSON fixture.
- Hardened source-file reading so a stale `errno` value cannot make an unrelated read failure look like the 64 MiB source-limit error; read, seek, allocation, and short-read failures now preserve or synthesize an accurate error.
- Added `tools/check_tree_sitter.py` to generate and corpus-test the Zed Tree-sitter grammar in an isolated temporary directory. CI and `npm test` now use it so grammar generation cannot overwrite the compiler's hand-written `src/parser.c`.
- Extended `make test` to verify that optional JSON, Binary I/O, TUI, Voice, and HTTP runtime flags are discovered in imported modules by recording the native compiler argument vector with fake `pkg-config`/`gcc` tools; the imported fixture also exercises `http.status()` directly.
- Removed a duplicated YSPM metadata assertion from the stable publishing workflow.
- Generated C now includes JSON, TUI, and Binary I/O runtime sections only when semantic analysis finds the corresponding built-ins in the program. Shared byte conversion and HTTP streaming helpers remain available where required.
- Added runtime-feature-gate checks to `make test`: minimal programs exclude unused JSON/HTTP/TUI/Binary/Voice sections, while imported modules retain every runtime feature required by their functions.
- Linked the Arabic documentation portal from the homepage and English docs portal; website checks enforce reciprocal navigation, Arabic language/direction metadata, and RTL prose with LTR code.
- Added native Windows interactive TUI acceptance coverage for raw key input, virtual-terminal output, cursor control, and console-mode restoration.
- Fixed Windows TUI shutdown so the cursor-show sequence is emitted before virtual-terminal processing is disabled, including automatic restoration when the caller does not explicitly show the cursor.
- Added an Arabic documentation portal and expanded the book with Arabic guides for compiler workflow, types, modules, CLI, JSON, HTTP/streaming, Binary I/O, Voice, TUI, GUI/Game2D, security, diagnostics, and Windows. Arabic prose uses RTL layout while code remains LTR. YLanglings, its CLI, exercises, hints, reference solutions, and book chapter remain English-only; compiler diagnostics, syntax, and API identifiers also remain in English.
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

Local verification was run on 2026-11-07 against a working tree based on commit `e63ae70a` (not a frozen release commit). The following passed: clean GCC and Clang builds on Linux; the full `make test` suite with GCC and Clang; `make sanitize` (AddressSanitizer and UndefinedBehaviorSanitizer); a 1,000-run Clang frontend fuzz smoke test; isolated Tree-sitter generation and corpus tests; VS Code and Neovim diagnostic/syntax checks; VS Code `.vsix` packaging; YLanglings unit tests and `verify-all`; the online book build; the website production build and integrity check; and YSPM package build plus archive-content inspection using a dynamic-link workaround. GitHub CI 30-sample benchmark runs #5079 and #5088 remain the recorded paired measurements.

The initial YSPM install/removal smoke could not complete because this host lacked static libc. On 2026-11-07, after installing `glibc-static` and `libxcrypt-static`, the statically linked YSPM package build and isolated install/compiler-smoke/removal test passed for `ylang-2.2.0-x86_64.yspkg`. This resolves the host setup issue for 2.2.0 only; the 2.3.0 candidate package still needs its own build/install/removal smoke. macOS builds, native Windows smoke, and the release-runner benchmark comparison have not been run for this working tree. These gates, and all remaining acceptance gates in [the release roadmap](ROADMAP-2.3.0.md), must be run against the exact final release candidate. No passing result is claimed for unverified gates.

The version file remains `2.2.0` until the 2.3 acceptance gates in [the release roadmap](ROADMAP-2.3.0.md) are complete.

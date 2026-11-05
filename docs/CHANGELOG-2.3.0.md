# YLang 2.3.0 — Development Notes

**Status: unreleased development notes.** YLang 2.2.0 remains the latest stable release. This document records work merged into the 2.3 development branch; it is not a release announcement and does not imply that validation gates have passed.

## Work integrated so far

- Fixed binary buffer cleanup so freeing a parent or nested slice invalidates and releases all descendant slice handles without leaving orphaned slots; expanded the dynamic-buffer regression fixture for root and non-root parent release.
- Preserved specific HTTP streaming callback errors when libcurl reports a generic transfer/write failure, so callers retain the actionable runtime diagnostic. Preserved specific Voice stream callback diagnostics when SDL_mixer reports a generic decoder setup failure.
- Fixed `json.at` so negative indices set the same explicit out-of-range diagnostic as other invalid array indices.
- Hardened `json.escape` and JSON builder keys to reject invalid UTF-8 instead of emitting malformed JSON strings; successful escaping and empty-container creation now clear stale JSON diagnostics.
- Removed the YTilawa application and its app-specific source, fixtures, and build steps. Shared TUI, Voice, HTTP streaming, and binary I/O facilities remain part of YLang.
- Added compiler resource limits for diagnostic output, module count, aggregate project source size, and import depth. Root files are size-checked before allocation; root and imported source files containing embedded NUL bytes are rejected.
- Added parser statement-nesting and code-generation recursion guards with dedicated diagnostics; recovery also handles unbraced nested statements, and C generation now fails closed on emission diagnostics.
- Fixed an HTTP response-header capacity check that could underflow an unsigned size calculation.
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

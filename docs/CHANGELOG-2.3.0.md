# YLang 2.3.0 — Development Notes

**Status: unreleased development notes.** YLang 2.2.0 remains the latest stable release. This document records work merged into the 2.3 development branch; it is not a release announcement and does not imply that validation gates have passed.

## Work integrated so far

- Removed the YTilawa application and its app-specific source, fixtures, and build steps. Shared TUI, Voice, HTTP streaming, and binary I/O facilities remain part of YLang.
- Added compiler resource limits for diagnostic output, module count, aggregate module source size, and import depth.
- Added parser statement-nesting and code-generation recursion guards with dedicated diagnostics; recovery also handles unbraced nested statements, and C generation now fails closed on emission diagnostics.
- Fixed an HTTP response-header capacity check that could underflow an unsigned size calculation.
- Added immutable raw-JSON builders: `json.object()`, `json.array()`, `json.set()`, `json.remove()`, and `json.push()`. `json.set` escapes keys and collapses duplicate copies of the replaced key.
- Refreshed the official homepage hero with a truthful code/native-build preview and improved the responsive terminal-menu example.
- Added `ylang help` and `ylang version` subcommands with matching option aliases and a full CLI reference.
- Expanded regression fixtures for module count/size limits, diagnostic output caps/control-byte escaping, deep unbraced parser nesting, JSON builder argument typing and duplicate keys, and exact-boundary oversized HTTP headers.
- Updated the online book build manifest, YSPM documentation payload, security/HTTP/JSON/CLI/error-code guides, and homepage navigation.

## Compatibility and limitations

The C backend remains the primary compiler backend. JSON builders operate on validated raw JSON text and return new owned strings; they do not create a typed JSON value tree or mutate their input. HTTP, HTTP streaming, Voice, TUI, GUI, NASM, named imports, and native Windows support retain their documented experimental boundaries.

LLVM completion, Typed IR expansion, complete ownership/borrowing and lifetime analysis, and a stable/full Windows platform promise are not part of this milestone.

## Validation status

The implementation work is still in progress. Builds, regression tests, parser fuzzing, AddressSanitizer/UndefinedBehaviorSanitizer, cross-platform checks, website/book production builds, editor packaging, YSPM install/remove checks, and reproducible benchmarks must be run against the final combined release candidate. No passing result is claimed here.

The version file remains `2.2.0` until the 2.3 acceptance gates in [the release roadmap](ROADMAP-2.3.0.md) are complete.

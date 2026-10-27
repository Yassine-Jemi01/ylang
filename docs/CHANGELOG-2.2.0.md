# YLang 2.2.0 — Development Changelog

**Status: development candidate. This file is not a release announcement, and 2.2.0 must not be published until the full CI matrix and the project owner's end-to-end acceptance test pass on the release commit.**

## Included in this development branch

- Adds the experimental HTTP API via optional libcurl: synchronous wrappers and generic `http.request(method, url, body, headers)` for GET, POST, PUT, PATCH, DELETE, and HEAD.
- Adds the first async I/O slice: `http.request_async(...)` returns a distinct `http_task`, `await task` consumes the task to return its response body, and `http.cancel(task)` cancels pending request state. Multiple transfers progress concurrently through libcurl multi while awaiting one.
- Adds `http.status()`, `http.response_headers()`, and `http.last_error()` for the most recent request.
- Adds `env.get(name)` so API credentials can be read from process environment variables without invoking a shell.
- Enforces request-method validation, HTTP header validation, a 16 KiB header limit, an 8 MiB response body limit, connect/overall timeouts, five-redirect maximum, HTTPS certificate/hostname verification, and HTTP/HTTPS-only redirects.
- Adds a source-aware experimental `ylang debug` command using `-O0`, debug symbols, and GDB on Linux or LLDB on macOS; generated C uses `#line` directives to map statements to original `.yl` locations.
- Adds integration coverage using an in-process loopback HTTP server, concurrent asynchronous requests, task cancellation, source-map checks, debugger launch checks, and unsupported-Windows-debugger checks.
- Updates VS Code and Neovim syntax highlighting for the `http` and `env` namespaces.
- Aligns the React homepage mark with the official documentation logo, adds direct links to the experimental HTTP/debugger reference pages, and preserves the previous static homepage as `classic.html` in the Pages deployment artifact.

## Compatibility and limitations

- Async/await currently applies only to HTTP task handles; general-purpose async functions, a background scheduler, and GUI/event-loop integration are not implemented.
- The HTTP API returns response bodies as strings. A JSON parser/serializer is not included; programs must currently construct/inspect JSON text themselves.
- HTTP support requires libcurl development files and `pkg-config` only when a program using `http.*` is linked. Ordinary programs do not need libcurl.
- The generic method names are case-sensitive and accept uppercase `GET`, `POST`, `PUT`, `PATCH`, `DELETE`, and `HEAD` only.
- HTTP status codes such as 404 are not transport failures. Callers must inspect `http.status()`; `http.last_error()` describes transport or request-configuration failures.
- Debugging is an initial CLI workflow, not a complete Debug Adapter Protocol implementation. VS Code starts GDB/LLDB in the integrated terminal; there is no dedicated Variables/Call Stack UI yet.
- Interactive debugger launch is supported only on Linux (GDB) and macOS (LLDB) in this branch. The Windows command fails early with an explicit unsupported message. The NASM backend has no source-level debugger contract.
- Source mapping points the native debugger at YLang file/line locations, but local variables and some scopes may still expose generated C names. This is not a sandbox or a Rust-level memory-safety guarantee.
- The official homepage still describes 2.0.0 as the current stable release while this branch is only a development candidate. No public release or deployment is performed by these changes.

## Required gates before stable release

1. All jobs on the exact candidate commit pass: GCC, Clang, macOS, Windows smoke tests, sanitizers, packaging, editor tests, book/site integrity, and the homepage build.
2. API integration tests cover GET, POST, PUT, custom headers, response headers, status codes, invalid headers, missing environment variables, and error behavior.
3. The owner completes one consolidated test round on the candidate build and confirms the expected behavior.
4. Version and release metadata are reviewed for the exact 2.2.0 release commit, then the release is tagged and published explicitly.

See `ROADMAP-2.2.0.md`, `HTTP.md`, `DEBUGGER.md`, and `DEBUGGER-SPEC.md` for the full design and known boundaries.

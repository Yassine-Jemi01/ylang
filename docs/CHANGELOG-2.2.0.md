# YLang 2.2.0 — Stable Release

YLang 2.2.0 expands the developer toolchain with experimental HTTP/API requests, asynchronous HTTP tasks, environment-variable access, and source-aware native debugging. The documented core language and default C-backend workflow remain the compatibility foundation.

**Compatibility boundary:** the HTTP client, HTTP-task async API, debugger integration, named imports, structs/enums, GUI/Game2D, NASM backend, native Windows path, and YSPM packaging have explicit experimental limitations. This release does not claim Rust-level memory safety, a security sandbox, general-purpose async functions, a built-in JSON parser/serializer, a complete VS Code Debug Adapter Protocol implementation, or Windows interactive debugging.

## Highlights

- **HTTP and API requests:** synchronous helpers and `http.request(method, url, body, headers)` for GET, POST, PUT, PATCH, DELETE, and HEAD through optional libcurl support.
- **Asynchronous HTTP tasks:** `http.request_async(...)` returns an `http_task`; `await task` consumes it and returns the response body; `http.cancel(task)` requests cancellation. The libcurl multi interface lets multiple transfers progress while tasks are awaited.
- **Response and error inspection:** `http.status()`, `http.response_headers()`, and `http.last_error()` expose the latest response status, headers, and transport/configuration error.
- **Environment access:** `env.get(name)` reads process environment variables without invoking a shell, allowing programs to obtain credentials without hard-coding them.
- **Security defaults:** request-method and header validation, a 16 KiB header limit, an 8 MiB response-body limit, connect and overall timeouts, HTTPS certificate/hostname verification, and restricted redirect behavior for requests carrying custom headers.
- **Source-aware debugger:** `ylang debug` builds with `-O0` and debug symbols, then launches GDB on Linux or LLDB on macOS. Generated C source mappings let supported breakpoints refer to original `.yl` locations.
- **Developer tooling:** updated VS Code and Neovim syntax support for `http` and `env`, a terminal-based VS Code debug command, and repeatable compile/runtime/binary-size benchmark tooling.
- **Documentation and website:** refreshed homepage, online book, reference pages, and candidate-acceptance tooling.

## Known limitations

- Async/await applies only to HTTP task handles. General async functions, a background scheduler, and GUI/event-loop integration are not implemented.
- HTTP response bodies are strings; a built-in JSON parser/serializer is not included.
- Programs that use `http.*` require libcurl development files and `pkg-config` when linking. Ordinary programs do not require libcurl.
- HTTP method names are case-sensitive and must be uppercase. HTTP status codes such as 404 are not transport failures; callers must inspect `http.status()`.
- Debugging is an initial CLI workflow, not a complete Debug Adapter Protocol implementation. Local-variable names and some scopes may reflect generated C. Interactive debugging is supported on Linux with GDB and macOS with LLDB; Windows and the NASM backend do not have a source-level debugger contract.
- Experimental extensions are not covered by the stable-core compatibility guarantee and do not provide a security sandbox or complete Rust-like memory safety.

## Build and verify

Build from source with GCC or Clang and GNU Make:

```sh
make
make test
```

For HTTP-enabled programs, install libcurl development files and `pkg-config`. See [HTTP and API Requests](HTTP.md), [the debugger guide](DEBUGGER.md), [the security model](SECURITY.md), and [the performance guide](PERFORMANCE.md) for requirements and details.

The candidate acceptance procedure is documented in [ACCEPTANCE-2.2.0.md](ACCEPTANCE-2.2.0.md). YLang 2.2.0 does not include Marketplace publication of the VS Code extension; the extension can be packaged locally as a `.vsix`.

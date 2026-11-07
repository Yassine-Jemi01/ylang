# The Standard Library

YLang's standard library provides built-in namespaces for common tasks. The available surface is defined by the implementation and its reference documents; do not assume that a similarly named function from C, Python, Rust, or another language exists in YLang or behaves the same way.

This chapter is an orientation guide. For exact function signatures, accepted argument types, error behavior, and backend support, consult the [Complete Language Specification](reference/language-spec.md) and the feature-specific references linked below.

## Available namespaces

The current documentation covers namespaces such as:

- `math` for supported mathematical operations.
- `string` for string inspection and explicit parsing/conversion helpers.
- `io` for console and file-related operations.
- `path` for path-related checks.
- `image` for supported image-related operations.
- `json` for parsing, validation, typed field access, escaping, and formatting. YLang 2.3.0 includes immutable raw-text builders; see [JSON support](reference/JSON.md).
- `env.get(name)` for reading a process environment variable without launching a shell.
- `http` for optional synchronous requests and bounded pull-based streaming through libcurl; see [HTTP and API Requests](reference/HTTP.md) and [HTTP Streaming](reference/HTTP-STREAMING.md).
- `terminal` for line-oriented console input/output, and `tui` for raw-key terminal applications; see [Terminal UI](tui.md).
- `voice` for local audio playback and callback-backed HTTP stream playback through SDL2_mixer; see [Voice and Audio Playback](voice-audio.md).

The namespaces do not all have the same maturity. Basic language operations and experimental additions should be evaluated separately. HTTP streaming, environment access, Voice/audio, and raw-key TUI are optional additions and are outside the stable-core compatibility guarantee.

## Strings and parsing

Text values often arrive from files, console input, or network responses. Do not treat text as a number simply because it contains digits. Use the documented parsing and validation helpers when converting text into a numeric value.

The documented helpers include `string.is_int(text)`, which checks whether the full text is a valid signed integer representation, and `string.parse_int(text)`, which parses integer text with failure handling. Consult the specification for exact accepted formats and failure behavior.

When handling untrusted or user-provided text, validate the result before using it in arithmetic or as an index, path, or other control value. For JSON, use `json.parse`, typed getters, and `json.last_error`; use `json.escape` when inserting user-provided text into a JSON string. The immutable `json.object`, `json.array`, `json.set`, `json.remove`, and `json.push` helpers build raw JSON text without introducing a dynamic JSON value tree.

## Files, paths, and environment

Use the documented `io` and `path` operations for file-related work instead of relying on the current working directory or hand-built path assumptions. Paths can be interpreted differently across platforms, so test the same operation on every platform your application claims to support.

The `env.get(name)` helper reads a process environment variable without launching a shell. Environment variables may contain missing, empty, or unexpected values; check the documented return behavior and validate the value before depending on it. Avoid storing secrets in source code or printing sensitive environment values to logs.

See [Binary I/O](binary-io.md) for byte-oriented file access and its current limits. HTTP streaming can copy raw response chunks into reusable byte buffers with `http.stream_read_buffer`; do not use the text-only `http.stream_read` API for binary payloads. If a text chunk contains an embedded NUL byte, `http.stream_read` records a stream error, terminates that stream, and discards queued text bytes.

## JSON documents
## Building JSON text in 2.3

YLang 2.3 adds immutable raw-text builders. The [JSON reference](reference/JSON.md) contains a complete example in which `json.object` and `json.array` create containers, `json.escape` encodes user text, and `json.set`, `json.push`, and `json.remove` return new owned strings. The original values are never modified in place. These builder functions are development additions and are not available in the stable 2.2.0 release.


The JSON API represents a document and extracted values as JSON text held in owned strings; it does not create a dynamic object tree. For input containing duplicate object keys, getters return the first value; `json.set` replaces that key and removes later duplicates, while `json.remove` removes all copies. Use `json.parse` or `json.valid` before trusting input, check optional keys with `json.has`, and inspect `json.last_error()` after a failed lookup or typed conversion.

YLang 2.2.0 introduced parsing, accessors, escaping, and pretty-printing. YLang 2.3.0 adds the immutable builders `json.object`, `json.array`, `json.set`, `json.remove`, and `json.push`. Builder values must be valid raw JSON fragments; use `json.escape` to construct a JSON string literal. Each builder returns a new owned string rather than mutating the original. See the [JSON reference](reference/JSON.md) for limits and exact behavior.

## HTTP requests

The HTTP namespace is an experimental integration and may require libcurl at build or runtime. Network access introduces failures that ordinary local calculations do not have: a server can be unreachable, return an error status, send unexpected content, or redirect a request.

Read [HTTP and API Requests](reference/HTTP.md) and the [Security Model](reference/SECURITY.md) before using these APIs. Do not assume that HTTP requests are asynchronous or that they provide a sandbox. Apply timeouts, validate responses, and avoid sending credentials to an untrusted destination.

## Backend and platform support

Do not assume every standard-library call is supported by the experimental NASM backend. The C backend remains the broad-coverage default. A source file that checks successfully may still encounter backend-specific restrictions during code generation.

For a library feature that matters to your project:

1. Find its exact reference page.
2. Confirm required native libraries and build dependencies.
3. Check whether your selected backend and platform support it.
4. Build a minimal example and test its success and failure paths.
5. Keep the example in your project's tests so regressions are easy to detect.

The [Compiler Error Codes](reference/error-codes.md) page includes diagnostics for unknown standard-library namespaces.

## Optional graphical helpers

GUI, Game2D, and Voice are optional and have their own SDL2 requirements. Voice additionally requires SDL2_mixer. See [Voice and Audio Playback](voice-audio.md) for setup and the current playback API. They are not required for ordinary command-line programs. Start with [GUI and Game2D](gui-game2d.md) if you need windows, input, or simple 2D drawing.

## Practice

Write a small program that converts a string containing an integer into a numeric value using the documented helper. Test both valid and invalid text. Then choose one path or environment operation and document what happens when its input is missing or unexpected.
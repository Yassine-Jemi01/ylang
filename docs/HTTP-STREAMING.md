# HTTP Streaming

**Status: development branch only.** The current pull-based implementation is not part of a published YLang release.

## API

```yl
function main() -> int {
    let http_stream stream = http.stream_open(
        "GET",
        "https://example.com/events",
        "",
        ""
    );

    while (!http.stream_done(stream)) {
        let owned_string chunk = http.stream_read(stream, 16384);
        if (string.length(chunk) > 0) {
            print(chunk);
        }
    }

    if (string.length(http.stream_error(stream)) > 0) {
        print(http.stream_error(stream));
    }
    print(http.stream_status(stream));
    http.stream_close(stream);
    return 0;
}
```

| Function | Result | Contract |
|---|---|---|
| `http.stream_open(method, url, body, headers)` | `http_stream` | Start a bounded pull-based request |
| `http.stream_read(stream, max_bytes)` | `owned_string` | Return up to the requested number of bytes as a NUL-terminated text string |
| `http.stream_read_buffer(stream, buffer, max_bytes)` | `int` | Overwrite a root `io.buffer` with the next raw bytes; return the byte count, or `-1` on error |
| `http.stream_done(stream)` | `bool` | True after transfer completion and buffer drain |
| `http.stream_status(stream)` | `int` | HTTP response status |
| `http.stream_headers(stream)` | `string` | Final response headers |
| `http.stream_error(stream)` | `string` | Per-stream transport/configuration error |
| `http.stream_close(stream)` | `void` | Cancel and release the stream |

## Progressive audio playback

The Voice runtime can take ownership of an HTTP stream and pass its bytes to SDL2_mixer through a custom forward-only SDL_RWops source:

```yl
function main() -> int {
    let string url = terminal.read_line("HTTPS audio URL: ");
    let http_stream stream = http.stream_open("GET", url, "", "");

    if (!voice.play_stream(stream)) {
        print(voice.last_error());
        voice.shutdown();
        return 1;
    }

    while (!voice.is_finished()) {
        tui.sleep_ms(20);
    }

    print(voice.last_error());
    voice.shutdown();
    return 0;
}
```

Calling `voice.play_stream(stream)` transfers ownership of the stream to Voice, even if playback setup fails. Do not call `http.stream_read`, `http.stream_read_buffer`, `http.stream_done`, or `http.stream_close` on that handle after handoff. Voice closes the HTTP stream when playback is stopped, fails, or reaches EOF. Pause, resume, stop, and volume continue to use the regular `voice.*` functions.

The audio reader is forward-only; seeking is not implemented. Actual MP3 support depends on the installed SDL2_mixer codec build. The callback-backed reader and HTTP bridge require runtime tests before being treated as production-ready.

## Current implementation limits

- C backend and libcurl only; progressive audio additionally requires SDL2_mixer.
- Each stream uses a fixed 64 KiB response buffer; the runtime applies libcurl pause/resume backpressure when the consumer falls behind.
- At most 16 active streams.
- The API is blocking and pull-based; it is not a general asynchronous scheduler.
- A stream is single-consumer and must be closed. Process shutdown also releases active streams.
- HTTP 4xx/5xx statuses are responses, not transport errors; inspect the status and read the body.
- Redirects and TLS verification follow the existing HTTP runtime policy. Connection setup has a 5-second timeout; the stream has no whole-transfer timeout so long recitations are not cut off after 15 seconds.

**Binary-safe path:** use `http.stream_read_buffer(stream, buffer, max_bytes)` with a root buffer created by `io.buffer_new(0)`. Each call overwrites that buffer with the next raw bytes and returns the exact byte count. Embedded NUL bytes and values from 0 through 255 are preserved. The caller owns the buffer and must call `io.buffer_free` when finished. Slices are rejected as destinations.

`http.stream_read` remains a text convenience API and is not safe for arbitrary binary payloads. The binary buffer path is now available, but the Voice runtime is not yet connected to an HTTP stream; progressive network MP3 playback is still a separate integration task.

## Verification

The loopback integration test covers delayed chunk delivery, partial reads, response metadata, empty bodies, 404 response bodies, and a 9 MiB transfer. These tests must pass in CI before the implementation is considered validated. TLS failure, timeouts, cancellation under load, concurrent streams, sanitizer coverage, and Windows/macOS runtime behavior remain separate verification gates.

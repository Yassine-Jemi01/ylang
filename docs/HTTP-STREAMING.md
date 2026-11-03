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
| `http.stream_read(stream, max_bytes)` | `owned_string` | Return up to the requested number of response bytes as a NUL-terminated string |
| `http.stream_done(stream)` | `bool` | True after transfer completion and buffer drain |
| `http.stream_status(stream)` | `int` | HTTP response status |
| `http.stream_headers(stream)` | `string` | Final response headers |
| `http.stream_error(stream)` | `string` | Per-stream transport/configuration error |
| `http.stream_close(stream)` | `void` | Cancel and release the stream |

## Current implementation limits

- C backend and libcurl only.
- Each stream uses a fixed 64 KiB response buffer; the runtime applies libcurl pause/resume backpressure when the consumer falls behind.
- At most 16 active streams.
- The API is blocking and pull-based; it is not a general asynchronous scheduler.
- A stream is single-consumer and must be closed. Process shutdown also releases active streams.
- HTTP 4xx/5xx statuses are responses, not transport errors; inspect the status and read the body.
- Redirects and TLS verification follow the existing HTTP runtime policy.

**Important binary-safety limitation:** `http.stream_read` currently returns a NUL-terminated `owned_string`. It is therefore only suitable for text payloads. It is not safe for MP3, arbitrary binary files, or data containing embedded NUL bytes. Do not pass its result to the audio decoder as if it were a byte buffer. A length-carrying byte-buffer API and an audio streaming bridge are still required before YTilawa can play network MP3 progressively.

## Verification

The loopback integration test covers delayed chunk delivery, partial reads, response metadata, empty bodies, 404 response bodies, and a 9 MiB transfer. These tests must pass in CI before the implementation is considered validated. TLS failure, timeouts, cancellation under load, concurrent streams, sanitizer coverage, and Windows/macOS runtime behavior remain separate verification gates.

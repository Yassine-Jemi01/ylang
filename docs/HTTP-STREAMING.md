# HTTP Streaming (YLang 2.3)

**Status: implemented on `feat/2.3-http-streaming`; not yet released.** The compiler, C runtime, and loopback integration coverage are present on this development branch. The feature is not available in a published YLang release until this branch is reviewed and merged.

## Goals

HTTP Streaming must deliver response bytes incrementally. It must not collect the complete response body in memory, and it must apply backpressure when the application is slower than the network. It reuses the existing libcurl security policy (HTTPS verification, HTTP/HTTPS-only protocols, bounded redirects, timeouts, and validated request headers).

The first implementation targets the C backend and libcurl. The existing buffered APIs remain unchanged.

## API

~~~ylang
fn main() -> int {
    let stream: http_stream = http.stream_open(
        "GET",
        "https://example.com/large-file",
        "",
        ""
    )

    while (!http.stream_done(stream)) {
        let chunk: owned_string = http.stream_read(stream, 16384)
        if string.length(chunk) > 0 {
            process(chunk)
        }
    }

    if string.length(http.stream_error(stream)) > 0 {
        print(http.stream_error(stream))
    }

    print(http.stream_status(stream))
    http.stream_close(stream)
    return 0;
}
~~~

| Function | Result | Contract |
|---|---|---|
| http.stream_open(method, url, body, headers) | http_stream | Start a request and return a process-local stream handle. |
| http.stream_read(stream, max_bytes) | owned_string | Wait for the next bytes, EOF, or error; return at most max_bytes. |
| http.stream_done(stream) | bool | True after transfer ends and all buffered bytes are consumed. |
| http.stream_status(stream) | int | HTTP response status for this stream. |
| http.stream_headers(stream) | string | Final response headers, newline-separated. |
| http.stream_error(stream) | string | Transport/configuration error, or empty on success. |
| http.stream_close(stream) | void | Cancel if active and release all stream resources. |

The handle has its own http_stream type. It must not be interchangeable with integers or http_task handles. A stream is single-consumer and must be closed; process shutdown also releases active streams. The current chunk type is `owned_string`, so this API is for text/UTF-8-style response data; it is not a binary-safe audio/video or arbitrary-file API because YLang strings are NUL-terminated. A length-carrying byte-buffer type is required before binary streaming can be supported safely.

## Runtime design

- Each active stream owns a dedicated CURLM transfer, pumped by stream_read.
- The libcurl write callback copies incoming bytes into a fixed-size per-stream buffer. If unread data occupies that buffer, return CURL_WRITEFUNC_PAUSE; resume only after the consumer drains data.
- Keep response buffering bounded to 64 KiB per stream plus the 16 KiB response-header limit, independent of response size. Up to 16 streams may be active at once.
- Preserve partial chunks when the caller asks for fewer bytes than are buffered.
- Do not expose pointers into libcurl-owned memory. Every returned chunk is an owned YLang string.
- Preserve final response headers across redirects and enforce the existing 5-second connect and 15-second total timeouts.
- A 4xx/5xx response is an HTTP response, not a transport error. Return its body and status normally.
- Reject invalid handles and handle double-close/read-after-close safely; monotonically increasing stream IDs are not reused during normal process lifetime.

## Current verification coverage

The loopback integration suite now checks delayed chunk delivery, partial reads, final headers, empty responses, 404 bodies, and a 9 MiB response (larger than the buffered API's 8 MiB limit). The runtime's fixed-size buffer provides the per-stream memory bound by construction. TLS failure, timeout, cancellation under load, interleaved streams, ASan/UBSan, and the NASM diagnostic still require CI or local verification.

## Compatibility and scope

This milestone does not add a general-purpose async scheduler or promise API. The stream API is blocking and pull-based: `stream_read` waits for the next text bytes, EOF, or failure. The application controls backpressure by deciding when to read again. `stream_done` becomes true only after the transfer has ended and all buffered bytes have been consumed. HTTP 4xx/5xx responses are not transport errors; inspect `stream_status` and read their response bodies normally. A future async stream API can be layered on the same bounded queue after the runtime task scheduling model is defined.

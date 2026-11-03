# HTTP Streaming (YLang 2.3)

**Status: implementation in progress.** This document defines the streaming contract; it does not claim that the API is already available in a released compiler.

## Goals

HTTP Streaming must deliver response bytes incrementally. It must not collect the complete response body in memory, and it must apply backpressure when the application is slower than the network. It reuses the existing libcurl security policy (HTTPS verification, HTTP/HTTPS-only protocols, bounded redirects, timeouts, and validated request headers).

The first implementation targets the C backend and libcurl. The existing buffered APIs remain unchanged.

## Proposed API

~~~ylang
import std.http

fn main() {
    let http_stream stream = http.stream_open(
        "GET",
        "https://example.com/large-file",
        "",
        ""
    )

    while !http.stream_done(stream) {
        let owned_string chunk = http.stream_read(stream, 16384)
        if string.length(chunk) > 0 {
            process(chunk)
        }
    }

    if string.length(http.stream_error(stream)) > 0 {
        print(http.stream_error(stream))
    }

    print(http.stream_status(stream))
    http.stream_close(stream)
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

The handle has its own http_stream type. It must not be interchangeable with integers or http_task handles. A stream is single-consumer and must be closed; process shutdown also releases active streams.

## Runtime design

- Use a dedicated CURLM transfer for each active stream, pumped by stream_read.
- The libcurl write callback copies incoming bytes into a fixed-size per-stream buffer. If unread data occupies that buffer, return CURL_WRITEFUNC_PAUSE; resume only after the consumer drains data.
- Keep memory bounded per stream (target: 64 KiB body buffer plus the existing 16 KiB header limit), independent of response size.
- Preserve partial chunks when the caller asks for fewer bytes than are buffered.
- Do not expose pointers into libcurl-owned memory. Every returned chunk is an owned YLang string.
- Preserve final response headers across redirects and enforce the existing 5-second connect and 15-second total timeouts.
- A 4xx/5xx response is an HTTP response, not a transport error. Return its body and status normally.
- Reject invalid handles and double-close/read-after-close safely; do not reuse stale stream IDs.

## Acceptance tests

1. Read a multi-megabyte response in small chunks and verify byte-for-byte output.
2. Verify peak buffered response memory stays within the per-stream bound.
3. Verify a delayed/chunked server response yields data before the server finishes sending it.
4. Verify partial reads preserve the unread suffix and no bytes are duplicated or dropped.
5. Verify EOF, empty response, 404 response, redirects, TLS failures, timeouts, malformed headers, and cancellation.
6. Open multiple streams and interleave reads; closing one must not affect the others.
7. Run the HTTP integration suite and ASan/UBSan tests; verify no leaks, use-after-free, or double-free.
8. Verify non-HTTP programs still build without libcurl and the NASM backend reports a clear unsupported-feature diagnostic if streaming is used.

## Compatibility and scope

This milestone does not add a general-purpose async scheduler or promise API. The first stable contract is a blocking, pull-based stream: the network transfer progresses while stream_read waits for the next chunk, and the application controls backpressure by deciding when to read again. A future async stream API can be layered on the same bounded queue after the runtime task scheduling model is defined.

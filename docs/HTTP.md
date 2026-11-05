# HTTP and API Requests (Experimental)

**Status: experimental development feature for YLang 2.3.** HTTP functions are implemented on the C backend behind an optional libcurl dependency. The API includes synchronous calls plus an initial asynchronous task API built on libcurl's multi interface; async/await is currently restricted to HTTP task handles, not general-purpose YLang functions.

## Streaming

Pull-based HTTP Streaming is implemented in the current 2.3 development tree. It provides a distinct `http_stream` handle, bounded 64 KiB per-stream response buffering, and a binary-safe byte-buffer read path. The Voice runtime can consume a stream through `voice.play_stream`. See [HTTP-STREAMING.md](HTTP-STREAMING.md) for ownership, limits, and verification status; this feature is not part of a published release yet.

## Requirements

Install libcurl development files and pkg-config only on machines that build programs using the HTTP namespace.

- Fedora: `sudo dnf install libcurl-devel pkgconf-pkg-config`
- Debian/Ubuntu: `sudo apt install libcurl4-openssl-dev pkg-config`
- macOS: `brew install curl pkg-config`; if Homebrew curl is keg-only, set `PKG_CONFIG_PATH="$(brew --prefix curl)/lib/pkgconfig:$PKG_CONFIG_PATH"`.
- Windows with MSYS2 MINGW64: `pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf`

The ordinary compiler and programs that do not call `http.*` do not require libcurl. `ylang check` validates HTTP calls without linking the HTTP runtime; native `ylang build` requires the development dependency when an HTTP function is used. The experimental NASM backend does not support HTTP calls, including streaming. Async tasks are process-local handles with a maximum of 32 in-flight slots; each task may be consumed once by `await`, and cancellation is not a remote rollback.

## API

| Function | Result | Purpose |
|---|---|---|
| `http.get(url)` | `owned_string` | GET request without custom headers |
| `http.get_with_headers(url, headers)` | `owned_string` | GET request with headers |
| `http.post(url, body)` | `owned_string` | POST request with `Content-Type: application/json` |
| `http.post_with_headers(url, body, headers)` | `owned_string` | POST with a custom header block |
| `http.request(method, url, body, headers)` | `owned_string` | Synchronous request using GET, POST, PUT, PATCH, DELETE, or HEAD |
| `http.request_async(method, url, body, headers)` | `http_task` | Queue a request without blocking on its response |
| `http.stream_open(method, url, body, headers)` | `http_stream` | Start a bounded pull-based response stream |
| `http.stream_read(stream, max_bytes)` | `owned_string` | Block until text bytes are available, EOF, or error |
| `http.stream_read_buffer(stream, buffer, max_bytes)` | `int` | Read raw bytes into a reusable root `io.buffer`, preserving embedded NUL bytes |
| `http.stream_done(stream)` | `bool` | True after transfer completion and buffer drain |
| `http.stream_status(stream)` | `int` | HTTP status for this stream |
| `http.stream_headers(stream)` | `string` | Final response headers for this stream |
| `http.stream_error(stream)` | `string` | Per-stream transport/configuration error |
| `http.stream_close(stream)` | `void` | Cancel and release this stream |
| `await task` | `owned_string` | Wait for a queued task and return its response body |
| `http.cancel(task)` | `bool` | Cancel a task that has not yet been awaited |
| `http.status()` | `int` | HTTP status from the most recent request |
| `http.response_headers()` | `string` | Final response headers, newline-separated |
| `http.last_error()` | `string` | Transport/configuration error for the most recent request, or an empty string after a successful transfer |

`http_stream` is a distinct handle type. `http.stream_status` and `http.stream_headers` wait until final response headers are available (including an allowed redirect chain), while leaving the response body unread. `http.stream_read` returns NUL-terminated `owned_string` chunks for text responses. For arbitrary binary data, use `http.stream_read_buffer(stream, buffer, max_bytes)` with a root buffer from `io.buffer_new(0)`; it overwrites the buffer and returns the exact byte count. This preserves embedded NUL and high-bit bytes. Passing a stream to `voice.play_stream(stream)` transfers ownership to the Voice runtime; do not use the stream handle again after handoff.

Pass headers as one header per line, separated by `\n`, for example `"Content-Type: application/json\nAuthorization: Bearer ..."`. Header names are validated and malformed headers are rejected. A task handle has the distinct `http_task` type, so the compiler rejects `await` or `http.cancel` on arbitrary integers. Await each task at most once. Cancellation frees its pending request state; using a consumed handle again reports an error/false result rather than reusing stale memory. Use `http.status()` to distinguish HTTP error responses such as 404 from transport failures: an HTTP 404 response is still a successful HTTP transfer and its body is returned. `http.response_headers()` returns the final response's headers as newline-separated lines, replacing intermediate response headers after redirects. `http.request(method, url, body, headers)` accepts only the uppercase methods GET, POST, PUT, PATCH, DELETE, or HEAD. Its `body` argument is used for POST/PUT/PATCH/DELETE; GET and HEAD ignore it.

Buffered responses are limited to 8 MiB. Streaming responses are not subject to that whole-body limit;Buffered `http.get`/`http.request` and `await` task results are text strings: if a response body contains an embedded NUL byte, the string result is empty and `http.last_error()` explains that binary-safe streaming is required. Use `http.stream_read_buffer` with an `io.buffer` for arbitrary response bytes.  each active stream uses a fixed 64 KiB body buffer (maximum 16 active streams). Buffered HTTP requests use a 5-second connection timeout and a 15-second whole-request timeout. Streaming requests retain the 5-second connection timeout but have no whole-transfer timeout, so long audio recitations are not truncated by a fixed request deadline. Requests without custom headers automatically follow at most five redirects, restricted to HTTP/HTTPS. A `Content-Type` header is permitted through redirects regardless of the field-name letter casing; if the header list includes any other header, automatic redirect following is disabled and the original 3xx status, body, and headers are returned. This avoids forwarding arbitrary credentials such as `Authorization` or `X-API-Key` to a redirect target. HTTPS certificate and hostname verification are enabled; the runtime does not offer an insecure TLS bypass. These safeguards help with reliability, but they do not make arbitrary URLs safe in every application or create a network sandbox.

## Reading API keys from environment variables

Use `env.get(name)` to read a process environment variable. It returns an empty string if the variable is missing; it never invokes a shell. Do not hard-code API secrets into examples, source control, or the compiler.

```ylang
function main() -> int {
    let string api_key = env.get("MY_AI_API_KEY");
    if (string.length(api_key) == 0) {
        print("Set MY_AI_API_KEY before running this program.");
        return 1;
    }

    let string headers = string.concat(
        "Content-Type: application/json\nAuthorization: Bearer ",
        api_key
    );
    let owned_string response = http.post_with_headers(
        "https://api.example.com/v1/chat/completions",
        "{\"model\":\"example-model\",\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]}",
        headers
    );

    print("HTTP status:", http.status());
    if (string.length(http.last_error()) > 0) {
        print("Request failed:", http.last_error());
        return 1;
    }
    print(response);
    return 0;
}
```

Replace the illustrative endpoint, model name, request JSON, and authorization scheme with those documented by the chosen provider. API response bodies are returned as strings. Use `std.json` to validate, parse, inspect, and format JSON documents; see [JSON support](JSON.md). Object/array construction helpers are tracked separately for the 2.3 development milestone.

## Error behavior

- Invalid URLs and malformed headers return an empty owned string, set `http.last_error()`, and set status to zero.
- DNS, TLS, timeout, connection, response-size, and other libcurl failures return an empty owned string and set `http.last_error()`.
- HTTP statuses such as 400, 401, 404, and 500 are not treated as transport errors. The response body and status are returned so the program can inspect them.
- `http.status()` and `http.last_error()` describe the most recent HTTP operation in the current process; they are not per-request response objects.
- Synchronous `http.get`/`http.request` calls block until the transfer completes. The `request_async`/`await` workflow allows several network transfers to progress concurrently, but `await` itself blocks the current thread while pumping libcurl. It is not a general scheduler and must not be described as non-blocking on GUI event loops. General async functions, structured task groups, and GUI/event-loop integration remain future work.
- `http.cancel(task)` cancels a queued or in-progress request; cancellation is not a remote rollback and may happen after the server has received the request.

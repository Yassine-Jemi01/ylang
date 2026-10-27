# HTTP and API Requests (Experimental)

**Status: experimental development feature for YLang 2.2.** HTTP functions are implemented on the C backend behind an optional libcurl dependency. The API is synchronous today; it does not implement async/await yet.

## Requirements

Install libcurl development files and pkg-config only on machines that build programs using the HTTP namespace.

- Fedora: `sudo dnf install libcurl-devel pkgconf-pkg-config`
- Debian/Ubuntu: `sudo apt install libcurl4-openssl-dev pkg-config`
- macOS: `brew install curl pkg-config`; if Homebrew curl is keg-only, set `PKG_CONFIG_PATH="$(brew --prefix curl)/lib/pkgconfig:$PKG_CONFIG_PATH"`.
- Windows with MSYS2 MINGW64: `pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf`

The ordinary compiler and programs that do not call `http.*` do not require libcurl. `ylang check` validates HTTP calls without linking the HTTP runtime; native `ylang build` requires the development dependency when an HTTP function is used. The experimental NASM backend does not support HTTP calls.

## API

| Function | Result | Purpose |
|---|---|---|
| `http.get(url)` | `owned_string` | GET request without custom headers |
| `http.get_with_headers(url, headers)` | `owned_string` | GET request with headers |
| `http.post(url, body)` | `owned_string` | POST request with `Content-Type: application/json` |
| `http.post_with_headers(url, body, headers)` | `owned_string` | POST with a custom header block |
| `http.request(method, url, body, headers)` | `owned_string` | Synchronous request using GET, POST, PUT, PATCH, DELETE, or HEAD |
| `http.request_async(method, url, body, headers)` | `http_task` | Queue a request without blocking on its response |
| `await task` | `owned_string` | Wait for a queued task and return its response body |
| `http.cancel(task)` | `bool` | Cancel a task that has not yet been awaited |
| `http.status()` | `int` | HTTP status from the most recent request |
| `http.response_headers()` | `string` | Final response headers, newline-separated |
| `http.last_error()` | `string` | Transport/configuration error for the most recent request, or an empty string after a successful transfer |

Pass headers as one header per line, separated by `\n`, for example `"Content-Type: application/json\nAuthorization: Bearer ..."`. Header names are validated and malformed headers are rejected. A task handle has the distinct `http_task` type, so the compiler rejects `await` or `http.cancel` on arbitrary integers. Await each task at most once. Cancellation frees its pending request state; using a consumed handle again reports an error/false result rather than reusing stale memory. Use `http.status()` to distinguish HTTP error responses such as 404 from transport failures: an HTTP 404 response is still a successful HTTP transfer and its body is returned. `http.response_headers()` returns the final response's headers as newline-separated lines, replacing intermediate response headers after redirects. `http.request(method, url, body, headers)` accepts only the uppercase methods GET, POST, PUT, PATCH, DELETE, or HEAD. Its `body` argument is used for POST/PUT/PATCH/DELETE; GET and HEAD ignore it.

Responses are limited to 8 MiB. Connections time out after 5 seconds and whole requests after 15 seconds. Requests without custom headers automatically follow at most five redirects, restricted to HTTP/HTTPS. The built-in `http.post` JSON `Content-Type` header is permitted through redirects; if the header list includes any other header, automatic redirect following is disabled and the original 3xx status, body, and headers are returned. This avoids forwarding arbitrary credentials such as `Authorization` or `X-API-Key` to a redirect target. HTTPS certificate and hostname verification are enabled; the runtime does not offer an insecure TLS bypass. These safeguards help with reliability, but they do not make arbitrary URLs safe in every application or create a network sandbox.

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

Replace the illustrative endpoint, model name, request JSON, and authorization scheme with those documented by the chosen provider. API response bodies are returned as strings. A dedicated JSON parser/serializer is not included in this milestone, so applications must currently interpret or construct JSON text themselves.

## Error behavior

- Invalid URLs and malformed headers return an empty owned string, set `http.last_error()`, and set status to zero.
- DNS, TLS, timeout, connection, response-size, and other libcurl failures return an empty owned string and set `http.last_error()`.
- HTTP statuses such as 400, 401, 404, and 500 are not treated as transport errors. The response body and status are returned so the program can inspect them.
- `http.status()` and `http.last_error()` describe the most recent HTTP operation in the current process; they are not per-request response objects.
- Synchronous `http.get`/`http.request` calls block until the transfer completes. The `request_async`/`await` workflow allows several network transfers to progress concurrently, but `await` itself blocks the current thread while pumping libcurl. It is not a general scheduler and must not be described as non-blocking on GUI event loops.
- `http.cancel(task)` cancels a queued or in-progress request; cancellation is not a remote rollback and may happen after the server has received the request.

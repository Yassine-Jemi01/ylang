# HTTP and Web API Support for YLang 2.0

**Status:** experimental synchronous HTTP client implemented on `v2/http-api-client`; JSON parsing, module imports, and the final typed response API are still planned. This branch is a development preview, not a stable YLang release.

## Goal

YLang programs should be able to consume HTTPS/REST APIs without invoking shell commands or writing C glue. The first milestone is an HTTP client for external services. Hosting HTTP endpoints is a separate follow-up milestone and is not implied by the client API.

## Implemented prototype

YLang programs can now make synchronous HTTP/HTTPS requests with these experimental global built-ins:

- `http_get(url)`
- `http_get_with_headers(url, headers)`
- `http_post(url, body)`
- `http_post_with_headers(url, body, headers)`
- `http_request(method, url, body, headers)`
- `http_status_code()` and `http_last_error()`

Headers are provided as newline-separated `Name: value` lines. A request returns the response body as a string; status/error accessors refer to the most recent request. The prototype supports GET, POST, PUT, PATCH, DELETE, and HEAD. It uses libcurl in the generated application and only links `-lcurl` when the source uses HTTP built-ins.

The client enables certificate and hostname verification, permits only HTTP/HTTPS request protocols, does not automatically follow redirects, times out connection attempts after 5 seconds and the full request after 15 seconds, and limits the buffered response to 10 MiB. Redirect responses such as 302 are returned to the program; inspect `http_status_code()` and explicitly issue the next request if needed. Automatic redirect following is disabled by default so caller-supplied authorization/custom headers are not silently replayed to a different URL. HTTP status codes such as 404 are not transport errors: inspect `http_status_code()`. A transport error returns an empty response body and text in `http_last_error()`.

This prototype has known API-design limitations: response status is global rather than carried by a response object; HTTP bodies are strings; and API functions are global built-ins rather than imported namespaced functions. A limited JSON parser/accessor set is now available (`json_get_raw`, `json_get_string`, `json_get_int`, `json_get_float`, `json_get_bool`, `json_has`, `json_array_length`, `json_array_item`, `json_quote`, and `json_last_error`), but a general JSON value model is still future work. Check status immediately after the request and do not put the request and status accessor in the same expression.

The prototype needs libcurl development files when compiling an application that uses it:
- Fedora: `sudo dnf install libcurl-devel`
- Debian/Ubuntu: `sudo apt install libcurl4-openssl-dev`



## Proposed user-facing model

The API should return a response value rather than requiring callers to read process-global "last status" state. The intended shape is:

```ylang
// Illustrative target syntax; requires the module and response/value work below.
let HttpResponse response = http.get("https://api.example.com/items");

if response.status >= 200 && response.status < 300 {
    let JsonValue payload = json.parse(response.body);
    // Read typed fields through checked JSON accessors.
}
```

The exact public syntax must be finalized alongside YLang's module/import and record/struct semantics. Avoid shipping a large set of unnamespaced global built-ins as the permanent API.

## Client milestone

The first usable release should include:

- HTTPS and HTTP request support for GET, POST, PUT, PATCH, and DELETE.
- Request headers and bodies, including JSON content type and bearer-token authorization.
- A response value containing the HTTP status code, response body, and transport error information.
- Explicit timeouts, response-size limits, and deterministic failure behavior.
- TLS certificate and hostname verification enabled by default.
- Redirects are not automatically followed by default; no shelling out to `curl`.
- A clear way to distinguish an HTTP error status (for example 404) from a transport failure (for example DNS or TLS failure).

A libcurl-backed runtime is currently used by the prototype. It must remain an optional build dependency: ordinary YLang programs that do not use the HTTP module must not suddenly require libcurl headers or libraries. The compiler should report a direct, actionable error when an HTTP-using program is built without the required development package.

## JSON parser prototype and remaining work

The generated runtime now includes a small JSON parser with checked accessors:

- `json_get_raw(json, key)` returns the raw JSON source for an object property. Pass the returned object/array string to another accessor to read nested data.
- `json_get_string(json, key)`, `json_get_int(json, key)`, `json_get_float(json, key)`, and `json_get_bool(json, key)` read typed scalar properties.
- `json_has(json, key)` checks for a top-level object property.
- `json_array_length(json)` and `json_array_item(json, index)` inspect a JSON array. Items are returned as raw JSON text.
- `json_quote(string)` encodes a YLang string as a JSON string literal.
- `json_last_error()` reports the last JSON parse/access error as a string.

The parser limits input to 10 MiB and nesting to 64 levels. It decodes valid `\\uXXXX` escapes and surrogate pairs; JSON strings containing U+0000 are rejected because YLang strings are NUL-terminated. Duplicate object keys currently resolve to the first matching key. These helpers are a prototype, not a full JSON object/value type, schema validator, or streaming parser.

Before a stable 2.0 release, broaden tests for malformed input, Unicode edge cases, duplicate keys, numeric boundaries, large inputs, and error-state behavior. Add typed JSON values only after the ownership and error models are stable.

## Safety and resource rules

- Never disable TLS verification by default.
- Do not silently follow redirects to non-HTTP schemes.
- Set connection and overall request timeouts.
- Bound response size and JSON nesting depth.
- Do not log authorization headers, API keys, or request bodies by default.
- Make response ownership compatible with YLang's move/drop model, including every success and error path.
- Keep network operations out of the compiler's own process; they run in the generated application.

## Tests required before marking the feature implemented

1. Local mock HTTP server tests for each supported verb, headers, request body, status, and response body.
2. Timeout, connection-refused, invalid URL, oversized response, and malformed JSON tests.
3. Type-checker tests for wrong argument types and invalid field access.
4. Sanitizer tests for response buffers, JSON values, moves, and drops.
5. Linux tests with GCC and Clang; Windows CI coverage before claiming Windows support.
6. Tests proving that a program without HTTP calls builds without libcurl installed.

## Suggested next steps

1. Add a typed response value that carries body, status, and transport error instead of process-global status state.
2. Expand the JSON edge-case suite, define a typed JSON value/error model, and settle the ownership rules for nested objects and arrays.
3. Integrate the client and JSON operations as importable `http` and `json` modules once multi-file imports are stable.
4. Extend the local mock-server matrix to exercise timeout, TLS error, response limit, all verbs, and header edge cases.
5. Consider an HTTP server/router only after the client, module, JSON, and error models are stable.

This milestone does not promise async I/O, WebSockets, automatic retries, cookie storage, or a web-server framework.

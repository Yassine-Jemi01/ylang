# std.json — JSON support

YLang provides a native JSON namespace in the C runtime. It has no third-party dependency.

## Quick start

```yl
function main() -> int {
    let owned_string document = json.parse("{\"name\":\"YLang\",\"version\":23}");
    print(json.get_string(document, "name"));
    print(json.get_int(document, "version"));
    return 0;
}
```

## API

| Function | Return type | Description |
|---|---|---|
| json.parse(text) | owned_string | Validate and retain a JSON document |
| json.valid(text) | bool | Check JSON syntax without retaining a copy |
| json.stringify(raw) | owned_string | Validate and copy an existing raw JSON value |
| json.pretty(raw) | owned_string | Format a JSON value with two-space indentation |
| json.object() | owned_string | Create an empty JSON object |
| json.array() | owned_string | Create an empty JSON array |
| json.set(object, key, raw_value) | owned_string | Return an object with a property added or replaced |
| json.remove(object, key) | owned_string | Return an object without the selected property |
| json.push(array, raw_value) | owned_string | Return an array with one value appended |
| json.get(document, key) | owned_string | Get an object's property as raw JSON |
| json.get_string(document, key) | owned_string | Get and decode a string property |
| json.get_int(document, key) | int | Get a signed 64-bit integer property |
| json.get_float(document, key) | float | Get a numeric property |
| json.get_bool(document, key) | bool | Get a boolean property |
| json.has(document, key) | bool | Test whether an object contains a key |
| json.array_length(array) | int | Count top-level array elements |
| json.at(array, index) | owned_string | Get an array element as raw JSON |
| json.escape(text) | owned_string | Encode a string as a JSON string literal |
| json.last_error() | string | Read the most recent JSON diagnostic |

## Representation and behavior

The parser, accessors, and formatter are part of the documented 2.2 standard-library surface. Immutable object/array builders are additions in the unreleased 2.3 development line; do not assume they are available in a 2.2.0 installation.

JSON documents and extracted raw values are represented as owned strings, not as a dynamic YLang value tree. This keeps the first implementation compatible with the current type system and makes it useful for HTTP responses and configuration files. json.get and json.at return an empty string on lookup failure; typed getters return their zero value and set json.last_error(). Check json.has before accessing optional object properties.

json.stringify currently validates and copies an already-serialized JSON value. `json.pretty` validates and formats an already-serialized value with two-space indentation. `json.object` and `json.array` create empty containers. `json.set`, `json.remove`, and `json.push` return new JSON text values; they do not mutate the original string. `json.set` replaces the requested key and collapses any duplicate occurrences of that same key into one property; unrelated properties are preserved. Keys are escaped automatically, including quotes and control characters. Values passed to `json.set` and `json.push` must already be valid JSON fragments (use `json.escape` for strings). Invalid fragments and non-object/non-array inputs return an empty owned string and set `json.last_error()`. Use `json.escape` when encoding a string value. This API is immutable: each builder returns a new `owned_string`, leaving its input unchanged. It is a raw-text builder, not a typed DOM; there are no object handles or in-place mutation.

## Safety limits

- Maximum input and generated output size: 16 MiB.
- Maximum nesting depth: 128 containers (objects/arrays). Scalar values at the deepest level are valid; a 129th nested container is rejected.
- Builder results are parsed again before being returned, so combining individually valid values cannot create a result deeper than the limit.
- Invalid JSON is rejected, including malformed numbers, escapes, control characters, trailing data, and unpaired UTF-16 surrogate escapes.
- Unicode \\uXXXX escapes are decoded by json.get_string; surrogate pairs are combined into UTF-8.
- Literal UTF-8 in JSON strings is validated; malformed, overlong, and surrogate UTF-8 sequences are rejected.
- `json.get_string` reports U+0000 as unsupported because YLang's current string representation is NUL-terminated.
- The API is synchronous. It does not perform file or network I/O itself; combine it with io.read_file and http.*.

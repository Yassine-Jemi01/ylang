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

JSON documents and extracted raw values are represented as owned strings, not as a dynamic YLang value tree. This keeps the first implementation compatible with the current type system and makes it useful for HTTP responses and configuration files. json.get and json.at return an empty string on lookup failure; typed getters return their zero value and set json.last_error(). Check json.has before accessing optional object properties.

json.stringify currently validates and copies an already-serialized JSON value. `json.pretty` validates and formats an already-serialized value with two-space indentation. Neither function constructs objects or arrays from YLang values. Use json.escape when encoding a string value. A future typed JSON value/DOM API can add object construction, mutation, and pretty printing without changing this raw-document representation.

## Safety limits

- Maximum input size: 16 MiB.
- Maximum nesting depth: 128.
- Invalid JSON is rejected, including malformed numbers, escapes, control characters, trailing data, and unpaired UTF-16 surrogate escapes.
- Unicode \\uXXXX escapes are decoded by json.get_string; surrogate pairs are combined into UTF-8.
- The API is synchronous. It does not perform file or network I/O itself; combine it with io.read_file and http.*.

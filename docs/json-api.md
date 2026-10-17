# JSON helper API (YLang 2.0 preview)

**Status:** initial implementation. These are global compiler built-ins backed by generated C runtime helpers; they are not yet an importable `json` module or a fully typed JSON value model.

## Example

```ylang
function main() -> int {
    let string response = "{\"user\":{\"name\":\"Ada\"},\"scores\":[10,20,30]}";

    let string user = json_get_raw(response, "user");
    print(json_get_string(user, "name"));

    let string scores = json_get_raw(response, "scores");
    print(json_array_length(scores));
    print(json_array_item(scores, 1));

"name with \"quotes\""
    return 0;
}
```

Expected output:

```text
Ada
3
20
"name with \\"quotes\\""
```

## Functions

| Function | Return type | Behavior |
| --- | --- | --- |
| `json_get_raw(json, key)` | `string` | Returns the raw JSON token for a property on a JSON object; useful for nested objects/arrays. |
| `json_get_string(json, key)` | `string` | Decodes a string-valued property. |
| `json_get_int(json, key)` | `int` | Reads an integer property; fractional or out-of-range values set an error and return zero. |
| `json_get_float(json, key)` | `float` | Reads a finite numeric property. |
| `json_get_bool(json, key)` | `bool` | Reads a boolean property. |
| `json_has(json, key)` | `bool` | Tests whether a top-level property exists. A missing key returns false without treating absence as a parse error. |
| `json_array_length(json)` | `int` | Returns the length of a JSON array. |
| `json_array_item(json, index)` | `string` | Returns an array item as raw JSON text. The index is zero-based. |
| `json_quote(text)` | `string` | Returns a JSON string literal with quotes, backslashes, and control characters escaped. |
| `json_last_error()` | `string` | Returns the most recent parse or accessor error, or an empty string after a successful operation. |

Pass nested objects/arrays by first retrieving their raw text using `json_get_raw`, then call the relevant accessor on that text. Object keys are matched at the current object's top level; dotted paths are not interpreted. Arrays must be accessed using `json_array_length` and `json_array_item`.

## Error and resource behavior

A missing property or wrong JSON value type sets the JSON error state. Numeric accessors return `0` / `0.0` on failure and the boolean accessor returns false; check `json_last_error()` immediately when these default values could be ambiguous. `json_has` returns false for a missing key without setting an error, but malformed input still sets one.

The parser accepts at most 10 MiB of JSON text and nesting up to 64 levels. It decodes standard JSON escapes, including valid `\\uXXXX` sequences and surrogate pairs. U+0000 is rejected because YLang's current `string` representation is NUL-terminated. Duplicate object keys resolve to the first matching occurrence. The parser does not yet expose a generic JSON object/value type, validate schemas, stream large documents, or provide object/array serialization.

JSON helpers do not require libcurl. Only generated programs that use HTTP functions need the HTTP client library.

## Verification

The repository's regression suite runs `tests/json-api.yl` and compares its output with `tests/expected-json-api.txt`. The API is experimental; expand edge-case tests before treating it as a stable 2.0 contract.

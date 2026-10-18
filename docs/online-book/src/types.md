# Types and Explicit Conversions

YLang declarations make types visible. The language does **not** silently convert arbitrary values between types.

The common built-in types include `int`, `float`, `bool`, `char`, and `string`. Use the supported type and expression forms in the [Language Specification](reference/language-spec.md).

## Convert intentionally

Use explicit conversion functions when you need a different representation:

- `to_float(integer)` converts an integer to a floating-point value.
- `to_int(float)` truncates toward zero and reports a runtime error for non-finite or out-of-range values.
- `to_string(value)` formats supported scalar values as a string.
- `string.is_int(text)` checks whether the full text is a valid signed integer representation.
- `string.parse_int(text)` parses integer text with failure handling.

These functions make a conversion visible at the call site instead of hiding it inside an assignment or expression. See the [Language Specification](reference/language-spec.md) for the precise behavior and supported cases.

## Type errors are useful

When a value has the wrong type, change the declaration, correct the expression, or use a documented explicit conversion. Avoid adding a conversion just to silence an error unless it preserves the intended meaning.

## Not implemented in this beta

`Option<T>` and `Result<T, E>` have design notes but are not implemented language constructs. Refer to [Option and Result Design](reference/OPTION-RESULT-DESIGN.md) for the proposal; do not use them as current syntax.

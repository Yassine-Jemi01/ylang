# Types and Explicit Conversions

YLang declarations make types visible. The language does **not** silently convert arbitrary values between types. This makes the intended representation visible in the source and helps the compiler identify incompatible operations before the program runs.

The exact type grammar and supported operations are documented in the [Complete Language Specification](reference/language-spec.md). This chapter explains how to think about types and how to make conversions intentionally.

## Common built-in types

The common built-in types include:

- \`int\` for integer values.
- \`float\` for floating-point values.
- \`bool\` for boolean values.
- \`char\` for character values.
- \`string\` for text.

Choose a type based on the value's meaning, not only on what makes an assignment compile. For example, text that happens to contain digits is still text until you parse it into a numeric value. Similarly, a numeric result should not be treated as a string until it has been formatted.

## Declare values with their types

A declaration states the type at the point where a value is introduced:

\`\`\`ylang
let count: int = 12;
let ratio: float = 0.5;
let enabled: bool = true;
let label: string = "items";
\`\`\`

These examples use YLang 3.0's immutable-by-default declaration syntax. Use `let mut name: Type` when a binding needs to be reassigned.

A type mismatch is not necessarily a compiler obstacle to work around. It may reveal that the program is mixing two different concepts. Before converting a value, ask whether the operation makes sense for the domain.

## Convert intentionally

Use explicit conversion functions when you need a different representation:

- \`to_float(integer)\` converts an integer to a floating-point value.
- \`to_int(float)\` truncates toward zero and reports a runtime error for non-finite or out-of-range values.
- \`to_string(value)\` formats supported scalar values as a string.
- \`string.is_int(text)\` checks whether the full text is a valid signed integer representation.
- \`string.parse_int(text)\` parses integer text with failure handling.

These functions make a conversion visible at the call site instead of hiding it inside an assignment or expression. Refer to the [Complete Language Specification](reference/language-spec.md) for the precise behavior and supported cases.

For example, when a program receives a string from a file or user input, do not assume it is a valid integer simply because it looks numeric. Validate or parse it using the documented API, and handle the failure path before using the result.

## Truncation and numeric boundaries

Converting a floating-point value to an integer can discard the fractional part. The documented \`to_int\` behavior truncates toward zero, so it is not the same as rounding to the nearest integer. A conversion can also fail for values that are non-finite or outside the supported integer range.

When a calculation depends on a boundary, test values around that boundary and handle runtime errors as specified. Avoid relying on accidental behavior from a particular C compiler or platform.

## Type errors are useful

When a value has the wrong type, use this process:

1. Read the entire compiler diagnostic, including its error code and source location.
2. Check the declared type of each value involved in the expression.
3. Decide whether the declaration is wrong or whether the expression is combining unrelated types.
4. If a conversion is semantically appropriate, use a documented conversion function explicitly.
5. Run \`check\` again, then build and test the corrected program.

Avoid adding a conversion merely to silence an error if it changes the meaning of the calculation.

## Not implemented in this release

\`Option<T>\` and \`Result<T, E>\` have design notes but are not implemented language constructs in the release documented here. Refer to [Option and Result Design](reference/OPTION-RESULT-DESIGN.md) for the proposal; do not use them as current syntax or assume that their proposed behavior is available.

## Practice

1. Declare an integer and convert it to a floating-point value using the documented function.
2. Convert a numeric value to a string for display.
3. Try parsing a valid integer string and an invalid one; observe how the documented API reports failure.
4. Test a floating-point value with a fractional part to understand truncation toward zero.

Use the compiler and specification together: the specification explains the intended contract, while a minimal test confirms how the current build behaves.
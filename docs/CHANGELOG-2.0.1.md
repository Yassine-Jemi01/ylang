# YLang 2.0.1 — Stable Patch Release

YLang 2.0.1 is a corrective release in the 2.0 line. It keeps the documented stable-core contract and default C-backend workflow, while making evaluation order, string literal handling, integer parsing, and release metadata more predictable.

## Compiler correctness

- The C backend now explicitly evaluates effectful binary operands and multi-argument call arguments from left to right, independent of GCC/Clang argument-evaluation choices.
- Generated C string literals escape question marks so C17 trigraph processing cannot rewrite text such as `??/`, `??=`, and `??)`.
- The lexer validates simple, octal, and hexadecimal escapes before handing source to the C compiler. Invalid escapes produce YLang diagnostics, and hexadecimal escapes are accepted in one-byte `char` literals.
- Non-void generated functions include a defensive fallback return to prevent misleading C compiler fallthrough warnings when exhaustive `match` statements are lowered to conditional code.
- Float printing uses a concise decimal representation so common values such as `0.1` are not displayed with unnecessary binary floating-point digits.

## Runtime and source loading

- `string.is_int` and `string.parse_int` reject leading or trailing whitespace rather than silently accepting a partial-looking integer representation.
- `path.basename` follows POSIX path separators; a backslash is an ordinary filename character.
- On POSIX systems, `image.open` prefixes relative paths beginning with a hyphen before invoking the system opener, avoiding option-like arguments without using a shell.
- Module imports are restricted to resolved `.yl` source files, preventing unrelated text files from being treated as YLang programs or echoed in diagnostics.
- The language specification now clarifies that strings are NUL-terminated and that embedded `\\0` cannot be represented as an embedded NUL character in the current string model.

## Release tooling

- Version markers, compiler fallback version, README, website, and book references are aligned to 2.0.1.
- The beta publication workflow builds mdBook and stages its output before running website integrity checks.
- Regression tests cover evaluation order, C trigraphs, escape validation, integer parsing, float display, loop return analysis, POSIX basename behavior, and non-`.yl` imports.

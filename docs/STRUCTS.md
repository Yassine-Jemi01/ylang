# Structs (experimental)

YLang's current struct milestone supports named fields with explicit types, positional construction, field reads, passing structs to functions, returning structs, and assignment between values of the same struct type.

```yl
struct Point {
    int x;
    int y;
}

function shift(Point p, int dx) -> Point {
    return Point(p.x + dx, p.y);
}

function main() -> int {
    let Point p = shift(Point(2, 7), 3);
    print(p.x, ",", p.y);
    return 0;
}
```

## Current limits

- Declare a struct before using it; its fields must have valid non-void types.
- Construct with positional arguments in declaration order: `Point(2, 7)`.
- Field access is read-only in this milestone; assigning to `p.x` is not supported.
- No recursive by-value fields, methods, inheritance, generic structs, field defaults, or named constructor arguments.
- The C backend supports structs; NASM parity has not been established.
- This is experimental until the implementation PR and its CI matrix are reviewed and merged.

See [YLang 2.0 design](YLANG-2.0-DESIGN.md).

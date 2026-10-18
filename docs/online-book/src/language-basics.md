# Program Structure

YLang uses explicit type names in variable declarations and function signatures. Most programs define a `main` function that returns an integer status.

```ylang
function add(int left, int right) -> int {
    return left + right;
}

function main() -> int {
    let int total = add(20, 22);
    print(total);
    return 0;
}
```

The exact supported syntax is defined by the [Language Specification](reference/language-spec.md). Use that reference whenever an example and the compiler disagree.

## Variables

A mutable local uses a declaration such as:

```ylang
let int score = 10;
score = score + 1;
```

A constant declaration uses `const`:

```ylang
let const string language = "YLang";
```

YLang checks assignments and uses of values against their declared types. A value must be initialized before it is read, and a constant cannot be reassigned.

## Control flow

Conditional statements and loops let a function choose and repeat work. Keep the expression types explicit and run `ylang check` after each change. Consult the specification for the complete grammar supported by this beta; do not assume that syntax from C, Python, or Rust is accepted just because it looks familiar.

## User-defined data

The 2.0 beta adds experimental structs and unit enums. These features are useful for small programs, but their limits and backend support are not the same as Rust's. Continue to the [Structs](reference/STRUCTS.md) and [Enums and Matching](reference/ENUMS.md) chapters for their current contracts.

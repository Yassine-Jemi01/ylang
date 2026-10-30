# YLang 3.0: Function and Variable Declaration Syntax

This note records the source syntax being introduced on the 3.0 development branch. It does not change the stable 2.2 language specification.

## Functions

The canonical declaration keyword is `fn`:

```ylang
fn add(int left, int right) -> int {
    return left + right;
}

fn main() -> int {
    print(add(20, 22));
    return 0;
}
```

The legacy `function` spelling remains accepted during the 3.0 transition so existing projects can migrate incrementally. New examples and diagnostics should prefer `fn`.

## Variables

The explicit mutable spelling is:

```ylang
let mut int counter = 0;
counter += 1;
```

Constants continue to use:

```ylang
let const int limit = 100;
```

For source compatibility, the existing `let int name` form remains mutable for now. It is a legacy spelling, not an immutable binding. This compatibility rule must not be silently changed: any future deprecation or default-immutability change requires a separately reviewed migration plan and diagnostics.

The parser accepts `mut` only in the declaration position immediately after `let`; it is not a general-purpose modifier in type, parameter, or expression syntax yet.

## Current implementation boundary

- `fn` and legacy `function` are recognized by the lexer.
- `let mut` is parsed as a mutable declaration.
- `let const` retains its existing constant semantics.
- Legacy `let` remains mutable.
- Compiler semantics, all backends, editor grammars, formatter behavior, and generated documentation still need end-to-end verification before this syntax can be called complete for 3.0.

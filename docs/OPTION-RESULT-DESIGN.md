# Option and Result design for YLang 2.0

Status: design proposal only. This document does not claim that `Option` or `Result` are implemented.

## Goals

- Represent absence and recoverable failure without magic sentinel values.
- Keep YLang's explicit typing rule: no implicit conversion between `int`, `float`, `string`, or other types.
- Make failure paths visible to the type checker.
- Keep the stable 1.1.x language behavior unchanged until the feature is implemented and tested.

## Recommended first implementation: tagged values, not generics

The current type system does not yet provide general type parameters. Do not pretend that `Option<T>` or `Result<T, E>` work until generic types, payload layout, and code generation are implemented.

For an incremental milestone, introduce compiler-internal tagged values with concrete payload types, then generalize only after the type representation can express type parameters safely. The surface syntax should be finalized before code changes.

Conceptual semantics:

- `Option<T>`: either `Some(value)` or `None`.
- `Result<T, E>`: either `Ok(value)` or `Err(error)`.
- Matching must be exhaustive, like the existing unit-enum `match` checks.
- Accessing a payload is legal only after the checker proves which variant is active.
- No implicit conversion is performed for payloads or errors.

## Proposed user-facing examples (illustrative, not currently compilable)

```yl
// Illustrative syntax only; do not use as a working example yet.
let Option<int> maybe_port = Some(8080);

match maybe_port {
    Some(port) => { print(port); }
    None => { print("no port"); }
}
```

```yl
// Illustrative syntax only; do not use as a working example yet.
let Result<int, string> parsed = parse_port("8080");

match parsed {
    Ok(port) => { print(port); }
    Err(message) => { print(message); }
}
```

## Implementation sequence

1. **Type representation**: define type identity for payload-bearing variants and ensure incompatible payload types cannot be mixed.
2. **Parser and AST**: parse generic type arguments and variant payload patterns without changing existing enum parsing.
3. **Semantic analysis**: validate constructors, payload types, duplicate arms, exhaustive matching, and payload binding scope.
4. **C backend**: emit a tagged representation with explicit payload storage and safe access only in matching arms.
5. **Assembly backend**: keep the feature disabled for NASM until parity tests exist; do not silently route it through an unverified fallback.
6. **Tests**: cover `Some`/`None`, `Ok`/`Err`, nested payload types, wrong payloads, duplicate/missing arms, and use of payloads outside a matching arm.
7. **Docs and release gate**: label the feature experimental until Linux GCC, Linux Clang, macOS Clang, sanitizers, and backend parity tests pass.

## Non-goals for the first milestone

- General user-defined generics.
- Implicit conversions or exception-like hidden control flow.
- Assuming all errors are strings.
- Claiming full assembly-backend support before executable tests pass.

## Acceptance criteria

- Stable 1.1.x examples remain unchanged.
- Every invalid payload construction is rejected with a targeted diagnostic.
- Exhaustiveness and payload scope are checked at compile time.
- The generated program has deterministic behavior for every variant.
- CI proves the same observable behavior on supported C compilers.

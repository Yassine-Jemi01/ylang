# YLang 2 — Ownership and Borrowing

**Status:** development preview. Basic owned-value moves are checked for strings and arrays, and call-scoped scalar borrows are supported. This is not a complete borrow/lifetime checker and is not a Rust-equivalent safety guarantee.

## Design rules

- `int`, `float`, `bool`, and `char` copy by value.
- `string` and the supported one-dimensional arrays are owned values. Passing a named value by value, initializing a binding from another named owner, or returning a named value moves it.
- The source binding becomes unavailable after a detected move. Assigning a fresh value back into that binding reinitializes it.
- `clone(value)` is the explicit way to obtain a separate string or array copy.
- A move from a const binding or global is rejected in supported contexts; clone the value when a separate copy is required.
- Borrow expressions may only be passed directly to matching function parameters; reference values cannot be stored or returned.

## Example

```ylang
function report(&int value) -> void {
    print(value);
}

function increment(&mut int value) -> void {
    value = value + 1;
}

function main() -> int {
    let int count = 41;
    report(&count);
    increment(&mut count);
    print(count);

    let string owner = "YLang";
    let string other = owner;       // owner is moved
    let string copy = clone(other); // explicit copy
    print(other, copy);
    return 0;
}
```

## Borrow subset

- `&T` is a shared read borrow; assigning to the borrowed parameter is rejected.
- `&mut T` is an exclusive mutable borrow.
- Borrow mode must match the parameter declaration.
- Conflicting shared/mutable borrows of the same binding in a single call are rejected, including the supported nested-call argument cases.
- Borrowing currently supports `int`, `float`, `bool`, and `char` only. Strings, arrays, globals, local reference bindings, and reference returns are not borrowable in this initial subset.

## Known gaps

The checker uses a first-pass moved flag with conservative branch/loop merging; it is not a full control-flow dataflow engine. There are still paths where cleanup is deferred until process shutdown, especially on early returns and strings stored within arrays. The array/string runtime uses a tracking registry as a fallback, not a proof that each owned value is deterministically released on every path. Borrow analysis cannot reason about hidden aliases through global variables or foreign code.

Before stable 2.x, ownership checking must become proper control-flow dataflow, every ownership transfer must have one well-defined lowering rule, owned values must be dropped exactly once on every exit (`return`, `break`, `continue`, normal scope exit), and the runtime needs sanitizer-backed tests for move/reinitialize/clone and arrays of strings.

Do not claim Rust-equivalent memory safety until the compiler, generated C, runtime, and supported interop boundary have been reviewed and extensively tested.

# YLang Ownership and Borrowing

**Status:** Scalar borrowing is implemented on `dev/lsp-foundation`. The complete owning-memory model is still a design target, not a current safety guarantee. This work does not change the stable YLang 1.0.0 contract on `main`.

## Goals

- Make aliasing and mutation visible at function boundaries.
- Reject conflicting shared and mutable borrows in the implemented subset.
- Keep borrows limited to a function call; references cannot be stored in locals or returned.
- Extend the design to heap-owned strings and arrays only after move, clone, and cleanup rules are implemented.
- Do not claim Rust-equivalent memory safety from a partial implementation.

## Value and ownership direction

`int`, `float`, `bool`, and `char` are copyable scalar values. `string` values on `dev/lsp-foundation` now move when initialized from another named string, assigned from another named string, passed to a by-value string parameter, or returned by name. `clone(text)` explicitly copies a string. The checker rejects a named local string used after a move and conservatively merges move states across branches/loops. The runtime still tracks heap strings and array buffers until process exit, so deterministic scope cleanup is not complete. String values placed into array literals move from their named local; moving a string out of an indexed array slot is rejected, while `clone(values[index])` copies it. Arrays remain experimental, and full Rust-equivalent ownership guarantees are not implemented.

## Borrowed function parameters

```ylang
function show(&int value) -> void {
    print(value);
}

function increment(&mut int value) -> void {
    value = value + 1;
}

function main() -> int {
    let int score = 41;
    show(&score);
    increment(&mut score);
    print(score); // 42
    return 0;
}
```

- `&T` is a shared borrow and permits reading but not assigning to the borrowed binding.
- `&mut T` is an exclusive mutable borrow and permits reading/writing through that parameter.
- The call site must match explicitly: `show(&score)`, `increment(&mut score)`.
- Borrow expressions may only appear as direct arguments to matching borrowed parameters.
- Borrows last for the duration of the call; no local reference variables, reference returns, stored references, raw pointers, or lifetime annotations are supported.
- Multiple shared borrows can coexist.
- Mutable and shared borrows of the same binding cannot overlap within one call, including nested calls in another argument. Reading or assigning to a binding during an active mutable borrow is rejected.
- Mutable globals cannot be borrowed because other functions may access the same global through a different name.
- The first implementation supports `int`, `float`, `bool`, and `char` only. Borrowing `string` and arrays is rejected until their ownership and lifetime behavior is implemented.

## Next milestones

1. Stabilize borrow-mode and conflict diagnostics with positive and negative tests.
2. Extend string move analysis to full control-flow and return paths; replace process-wide tracking with deterministic cleanup on all control-flow exits.
3. Replace process-lifetime f-string buffers with deterministic ownership/cleanup.
4. Harden array ownership, including moves into literals, indexed string copies, and complete drop/cleanup behavior.
5. Enable borrowing of strings and arrays only after lifetimes and cleanup are covered by tests.
6. Update Tree-sitter parser generation and Neovim integration to the new grammar, then run the full platform pass.

## Safety boundary

The C backend and runtime remain part of the trusted implementation. This first borrowing feature checks aliasing around function calls, but it is not a complete ownership/lifetime checker and does not prove memory safety. Generated-code bugs, unchecked runtime operations, or unsupported features can still cause unsafe behavior.

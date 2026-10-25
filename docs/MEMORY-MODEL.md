# YLang Memory Model — RFC

Status: experimental, partial implementation on `feature/ownership-memory-v3`. The compiler retains static `move(name)` checks for legacy `string` values and adds a separate `owned string` type with explicit cloning, runtime moves, lexical cleanup, and local `&string` / `&mut string` borrow checks. Borrowed function parameters/returns, globals, struct fields, and general ownership-aware function signatures are still unimplemented.

## Goals

- Keep the default language free of a tracing garbage collector and periodic GC pauses.
- Free owned heap allocations deterministically when their owner leaves its lifetime.
- Reject use-after-move and conflicting local borrows at compile time.
- Preserve checked integer arithmetic and array bounds checks unless the compiler proves a check unnecessary.
- Keep the stable 2.0.x language behavior unchanged while this feature remains experimental.

## Current experimental behavior

### Legacy `string`

Legacy `string` remains represented by C-string pointers. Some generated runtime strings are retained until process exit, so they may alias. `move(name)` on a legacy string is only a static binding operation and does not free memory at scope exit. Do not treat legacy strings as owned values.

### Owned `owned string`

```yl
let owned string source = string.own("YLang");
let owned string destination = move(source);
print(destination);
```

- `string.own(value)` allocates an independent copy from a `string`, `owned string`, or borrowed view.
- `move(name)` transfers an owned pointer and clears the previous owner at runtime. The source binding becomes unavailable.
- Owned local values are dropped in reverse declaration order at normal block exit and before `return`, `break`, or `continue` exits a scope.
- Reassignment evaluates the replacement first, frees the previous value, then stores the replacement.
- Optional `YLANG_MEMORY_STATS=1` diagnostics report legacy tracked allocations and owned-string allocation/free/live-byte counters. These are requested payload measurements, not process RSS or allocator overhead.

### Local borrows

```yl
let owned string owner = string.own("YLang");
{
    let &string left = borrow(owner);
    let &string right = borrow(owner);
    print(left, string.length(right));
}
{
    let &mut string exclusive = borrow_mut(owner);
    print(exclusive);
}
let owned string transferred = move(owner);
```

- Multiple `&string` shared borrows may coexist.
- `&mut string` is an exclusive borrow of a non-const `owned string`; a second borrow conflicts while it is active.
- The owner cannot be moved or replaced while any tracked borrow is active. Direct owner reads are rejected while an exclusive borrow is active.
- Borrow variables must be initialized with `borrow(owner)` or `borrow_mut(owner)`, cannot be reassigned, and are scoped lexically. Nested scopes allow a borrow to end before moving its owner.
- Borrow types are not allowed in globals, struct fields, function parameters, or function return types in this milestone.
- The language's current string API is immutable, so `&mut string` currently reserves exclusive access but does not expose byte mutation. It can be read through `print`, f-strings, comparisons, and built-in string helpers.

## Safety limits

This is not a complete Rust-like memory-safety guarantee. Borrow syntax is supported for local string views, but general function-signature annotations, borrowed parameters/returns, reborrowing, and escape analysis across user-defined APIs are not implemented. The experimental NASM backend rejects owned and borrowed string types. Do not let foreign C code retain a pointer to a local owned string or borrowed view.

## Verification

The regression suite covers successful owned string cloning/moving, scope and early-exit cleanup, zero live owned allocations after cleanup, shared-borrow coexistence, exclusive-borrow conflicts, move/reassignment while borrowed, and rejection of borrow values escaping their supported lifetime. Run `make test`, `make sanitize`, and cross-platform CI before merging.

## Future work

1. Define ownership/borrow contracts for function parameters, return values, and imports.
2. Add mutation operations before claiming that `&mut string` provides useful mutation semantics.
3. Expand to owned structs/containers after nested destruction and exception/error paths are covered.
4. Keep measuring throughput and live/peak memory with equivalent safety checks.

# YLang 2 — Memory and Arrays

**Status:** the current development branch implements one-dimensional typed arrays and initial owned-value moves. The memory model remains a preview and needs more deterministic cleanup before a stable safety promise.

## Arrays

Supported array types are `int[]`, `float[]`, `bool[]`, `char[]`, and `string[]`. Arrays are dynamically allocated, homogeneous, one-dimensional values. Nested arrays are currently rejected.

```ylang
let int[] scores = [10, 20, 30];
scores[1] = 99;
scores = append(scores, 40);
print(scores[3]);

let int[] other = clone(scores);
other[0] = 7;
```

- Indexes start at zero and are runtime-checked.
- `len(array)` gives the element count.
- `append(array, value)` consumes the array value and returns a new value with the element appended. Store the result back in the variable.
- `clone(array)` creates a separate array buffer. Nested arrays do not exist, so the initial clone operation only needs to handle scalar elements and immutable string values.
- Global array initializers are currently not supported. Declare arrays in a function body.

## Ownership and strings

Strings are immutable NUL-terminated byte strings. A named string or array is moved by supported assignments, by-value function arguments, and returns. `clone(value)` explicitly creates a separate copy. The compiler rejects detected reads after moves, moves from const owners in supported contexts, and some moves from globals.

String literals can refer to static storage. Dynamically generated strings (including formatted strings and input) are tracked by the runtime. Normal local block exits drop owned local strings and array buffers; a process-exit registry provides fallback cleanup. Not all early control-flow exits currently emit deterministic drop code, and strings stored in string arrays may remain tracked until shutdown. This is a known memory-management gap.

## Runtime checks

The current runtime checks negative/out-of-range indexes, array size/capacity overflow, allocation failure, signed integer overflow, and division by zero. Input line length is capped at 1 MiB. `len(string)` returns byte length, not Unicode scalar count.

## Before a stable 2.x release

1. Complete move-state analysis across branches, loop iterations, reinitialization, returns, and function parameters.
2. Emit deterministic cleanup for all owned values on normal exit, `return`, `break`, and `continue`.
3. Add explicit runtime tests for arrays of strings, move/clone cycles, self-assignment, cleanup after early return, and repeated allocations in loops.
4. Test with GCC and Clang plus AddressSanitizer and UndefinedBehaviorSanitizer where available.
5. Add slices, nested arrays, or generic collections only after the ownership model is proven sufficient; these are not current features.

YLang generates C, so correctness of the code generator/runtime remains part of its trusted implementation. Bounds checks and regression tests reduce risk but do not prove total memory safety.

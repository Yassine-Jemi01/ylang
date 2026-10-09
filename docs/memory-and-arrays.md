# YLang Memory and Arrays Design

**Status: proposed design for the next language iteration.** This document does not change the stable YLang 1.0.0 specification or promise that the feature is implemented. Review the semantics before implementing the compiler/runtime changes.

## Goals

- Make array behavior predictable without requiring users to manage memory manually.
- Avoid dangling references and out-of-bounds memory access in ordinary YLang programs.
- Keep the rules implementable in the current C17 code-generation architecture.
- Define cleanup, copying, function arguments, and return values before adding heap-backed collections.
- Keep platform-specific details out of language semantics; use a portable C runtime and test it on Linux first.

YLang should not claim to be safer than Rust. These design choices can reduce particular classes of bugs, but safety depends on the implementation, generated code, runtime checks, and testing.

## 1. Value categories

### Scalar values

`int`, `float`, `bool`, and `char` are ordinary value types. Assignment and parameter passing copy the value. Their storage follows normal local/global variable lifetime rules.

### Strings

Strings remain immutable. A string value refers to immutable bytes; assigning or passing a string cannot mutate another string's contents. String literals may use static storage. Dynamically created strings (including f-string results) must be managed and released automatically rather than accumulating until process exit.

The proposed runtime uses reference-counted string storage. This is deterministic and fits the initial feature set because strings cannot point back to arrays or other mutable objects. The exact representation remains an implementation detail.

### Arrays

An array is a dynamically sized, homogeneous, one-dimensional sequence. The proposed type spelling is `T[]`, for example `int[]` or `string[]`. Arrays are values with copy-on-write storage:

- Copying an array creates an independent array value.
- Copies may share the same backing storage while nobody mutates it.
- Before a mutation, the runtime detaches the backing storage if it is shared.
- A mutation of one value never changes another copied value.
- The compiler/runtime manages allocation and release automatically.

This provides value semantics without eagerly copying every element on every assignment. The array buffer is reference-counted. When a buffer is detached, string elements in the new buffer acquire their own references; when a buffer is finally released, its string elements are released.

Nested arrays (such as `int[][]`) are deferred initially. The first implementation supports element types `int`, `float`, `bool`, `char`, and `string`. Arrays of `void`, and arrays whose element type is another array, are rejected. Deferring nested arrays avoids reference cycles and keeps the first memory model tractable.

## 2. Lifetime and automatic memory management

- Each local binding owns its string/array value for its lifetime.
- Assignments, arguments, return values, and temporary expressions follow one documented ownership convention in generated C.
- Leaving a scope releases values owned by that scope.
- Every control-flow exit must perform the required cleanup: normal block exit, `return`, `break`, and `continue`.
- Function return values must remain valid after the callee's locals are released.
- Static string literals are never freed. Dynamic strings and array buffers are released when no value refers to them.
- Allocation failure and impossible allocation-size overflow produce a defined runtime error and nonzero exit status; the program must not continue with a null or invalid buffer.

Reference counting is chosen over a tracing garbage collector for the first array implementation. Because nested arrays are deferred and strings are immutable, the initial value graph cannot contain reference cycles. If cyclic heap objects are added later, the memory strategy must be reconsidered before those types ship.

No user-visible manual `free`, raw pointers, or nullable array handles are part of this proposal.

## 3. Array syntax

### Declaration and literals

```ylang
let int[] scores = [10, 20, 30];
let string[] names = ["Ada", "Linus"];
let int[] empty = [];
```

Array literals must have elements of one identical type. There are no implicit element conversions. An empty literal has no inferable element type, so it requires an explicit array type from its declaration or another unambiguous type context.

### Indexing and mutation

```ylang
let int[] scores = [10, 20, 30];
print(scores[0]); // 10
scores[1] = 99;
```

Indexes are zero-based. Negative indexes are invalid. Every read and write checks `0 <= index < len(array)`; out-of-range access reports a runtime error instead of performing an unchecked C access.

### Length and append

```ylang
let int[] scores = [10, 20, 30];
print(len(scores)); // 3
scores = append(scores, 40);
```

`len(array)` returns the number of elements. `append(array, value)` returns a new array value with the appended element and does not mutate the argument's value. The result can reuse unique storage internally when this preserves the value semantics. A first release does not need `pop`, arbitrary insertion/removal, or a rich collection-method API; those can follow after the core rules are stable.

The returned length must fit YLang's documented integer range. Capacity growth, allocation overflow, and out-of-memory failures must be checked in the runtime.

## 4. Copying and function boundaries

Array parameters are passed by value. Mutating a parameter changes that local value, not the caller's value. The caller receives a change only by using a returned array value.

```ylang
let int[] original = [1, 2, 3];
let int[] changed = original;
changed[0] = 99;

print(original[0]); // 1
print(changed[0]);  // 99
```

Array-returning functions use the same type spelling:

```ylang
function add_score(int[] scores, int value) -> int[] {
    return append(scores, value);
}
```

Constants are deeply immutable for arrays: `let const int[] scores = [1, 2];` cannot be reassigned, indexed for mutation, or passed to an operation that mutates that value in place. Since `append` returns a value rather than mutating its argument, its result can be used to initialize a different mutable binding.

## 5. Runtime errors

The initial array runtime must define stable behavior for:

- Negative indexes and indexes at or beyond the array length.
- Array-length and allocation-size overflow.
- Allocation failure.
- Wrong element types and invalid array operations caught by semantic analysis.

Compile-time type errors should be reported by `ylang check`; bounds checks happen at runtime. Runtime errors use a nonzero status consistent with the existing runtime-error convention (currently status 70) and a clear message identifying the operation.

## 6. Required implementation order

1. Add this design to the language specification only after its semantics are accepted.
2. Define ownership helpers and tests for managed strings/temporaries before or alongside arrays; remove the current process-lifetime growth behavior of repeated f-string construction.
3. Add array AST representation, parsing, and source-located type diagnostics.
4. Add runtime helpers for allocation, retain/release, copy-on-write detachment, indexing, and append.
5. Generate cleanup for all lexical scopes and control-flow exits, including function calls and returns.
6. Add Linux regression tests for copy independence, strings inside arrays, nested scopes, function parameters/returns, const behavior, type errors, empty arrays, bounds errors, and memory cleanup.
7. Update the Tree-sitter grammar, VS Code syntax/language server, and Neovim integration to match the accepted grammar. Their full cross-platform validation is a later compatibility phase; existing CI remains an early warning.

## 7. Explicitly deferred

- Nested or multidimensional arrays.
- User-defined structs/classes and arrays of user-defined types.
- Raw pointers, references, and manual memory management.
- A tracing garbage collector.
- Slices/views that borrow an array's storage.
- Multithreaded mutation and thread-safe reference counting.
- String indexing/length semantics for Unicode scalar values.
- Promises of Rust-level memory safety.

The C backend means compiler/runtime correctness remains essential. Bounds checks and automatic lifetime management are goals, not proof that generated code is free from all undefined behavior or memory-safety bugs.

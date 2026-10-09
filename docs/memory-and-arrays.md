# YLang Memory and Arrays Design

**Status:** Proposed for a future language iteration. See [Ownership and Borrowing](ownership-and-borrowing.md) for the chosen direction and the current, limited scalar-borrowing implementation. The stable YLang 1.0.0 contract on `main` is unchanged.

## Design direction

YLang should use compiler-checked ownership for heap-owned values rather than relying only on reference counting or a garbage collector. The currently implemented `&T` / `&mut T` feature covers scalar function parameters only; it is not a complete memory model.

### Scalar values

`int`, `float`, `bool`, and `char` are copyable values. Their assignment and by-value parameter passing copy the scalar.

### Owned strings

`string` values are planned to be owned and moved by default. The compiler will reject use after move, and callers will explicitly request cloning when they need two independent owners. String literals may use static storage, but the source-level rules must not depend on whether an implementation chose static storage or allocated bytes. Dynamic strings, including f-string results, must be released deterministically when their owner ends.

The current C backend keeps f-string formatting buffers until process exit. Replacing that mechanism with deterministic ownership/cleanup is a prerequisite to claiming the string type is covered by the new safety model.

### Arrays

An array is planned as a dynamically sized homogeneous one-dimensional sequence, with type spelling `T[]`, such as `int[]` or `string[]`.

- Array values are owned and move by default; assignment does not silently create another owner.
- Copying requires an explicit operation with a documented cost. A future implementation may use copy-on-write internally only if it preserves the language's explicit ownership and clone semantics.
- Indexes are zero-based. Reads and writes must check `0 <= index < len(array)` unless the compiler can prove the check redundant.
- `len(array)` returns the length.
- `append(array, value)` should have documented move/ownership behavior. It may consume and return the array value to avoid repeated copies.
- All elements have one type. Implicit element conversions are not performed.
- Initially support one-dimensional arrays of `int`, `float`, `bool`, `char`, and `string`. Nested arrays are deferred until the ownership/drop model is proven.

## Function boundaries and lifetimes

By-value parameters receive owned values through moves for non-copyable types. Borrowed parameters use `&T` for shared read access and `&mut T` for exclusive access. References may not outlive their owner. Initially, references should be limited to function-call parameters, with no reference returns or storage; local references and lifetime inference can be introduced only after the simpler model is tested.

Every owned value must be cleaned up exactly once along each executed path. This includes normal scope exit, early returns, `break`, `continue`, and temporary expressions. A returned owned value must remain valid after the callee exits.

## Runtime failures

Array operations must define behavior for negative/out-of-range indexes, capacity/length overflow, and allocation failure. Those failures must be detected before unsafe C memory operations. Compile-time type/ownership violations are rejected by `ylang check`; dynamic bounds and allocation failures produce a documented nonzero runtime exit status.

## Implementation order

1. Validate scalar-borrow checking and its regression tests.
2. Implement ownership-state dataflow and move-after-use errors for owned strings.
3. Replace process-lifetime f-string tracking with deterministic ownership and cleanup.
4. Add typed array declarations/literals, then index reads/writes, `len`, and append.
5. Add copy/move/parameter/return semantics and cleanup on every control-flow exit.
6. Test positive and negative ownership cases, runtime bounds, allocation failures, and string/array behavior.
7. Update language docs, Tree-sitter, VS Code, and Neovim together with the accepted syntax.
8. Run the full Linux/Windows compatibility pass after the language core stabilizes.

## Explicitly deferred

- Nested arrays and user-defined heap types.
- Raw pointers, reference returns, local reference variables, and explicit lifetime annotations.
- A tracing garbage collector and cyclic object graphs.
- Thread-safe shared mutation.
- Rust-equivalent memory-safety claims.

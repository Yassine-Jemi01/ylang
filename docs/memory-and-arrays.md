# YLang Memory and Arrays Design

**Status:** The development branch implements a first slice: typed one-dimensional arrays, context-typed empty and non-empty literals, checked indexing, `len`, `append`, `clone`, and move/use-after-move checks for named array values. The stable YLang 1.0.0 contract on `main` is unchanged.

## Design direction

YLang should use compiler-checked ownership for heap-owned values rather than relying only on reference counting or a garbage collector. The currently implemented `&T` / `&mut T` feature covers scalar function parameters only; it is not a complete memory model.

### Scalar values

`int`, `float`, `bool`, and `char` are copyable values. Their assignment and by-value parameter passing copy the scalar.

### Owned strings

`string` values move by default when transferred from a named local into another variable, a by-value function parameter, or a return. `clone(text)` creates an independent copy. The checker rejects use after move and conservatively merges move state across branches and loops. String literals may use static storage, while dynamic strings and f-string results are currently tracked until process exit; deterministic scope cleanup is still required before claiming a complete ownership model.

The current C backend keeps dynamic strings, f-string formatting buffers, and array backing buffers in a runtime registry until process exit. This avoids freeing them too early, but it is not deterministic scope cleanup and means long-running programs may retain all prior dynamic allocations.

### Arrays

An array is a dynamically sized homogeneous one-dimensional sequence, with type spelling `T[]`, such as `int[]` or `string[]`. The initial implementation supports `int`, `float`, `bool`, `char`, and `string` elements.

- Array values are owned and move by default; assignment does not silently create another owner.
- Copying requires an explicit operation with a documented cost. A future implementation may use copy-on-write internally only if it preserves the language's explicit ownership and clone semantics.
- Indexes are zero-based. Reads and writes must check `0 <= index < len(array)` unless the compiler can prove the check redundant.
- `len(array)` returns the length.
- `append(array, value)` appends to a named mutable array variable and checks the element type. Appending a named string to a `string[]` moves that string value.
- All elements have one type. Implicit element conversions are not performed.
- Empty array literals such as `[]` are supported when context provides the element type, for example `let int[] values = [];`. An empty literal without a contextual array type is rejected. Nested arrays are deferred until the ownership/drop model is proven. Arrays currently print a length summary rather than all elements.

## Function boundaries and lifetimes

By-value parameters receive owned values through moves for non-copyable types. Borrowed parameters use `&T` for shared read access and `&mut T` for exclusive access. References may not outlive their owner. Initially, references should be limited to function-call parameters, with no reference returns or storage; local references and lifetime inference can be introduced only after the simpler model is tested.

Every owned value must be cleaned up exactly once along each executed path. This includes normal scope exit, early returns, `break`, `continue`, and temporary expressions. A returned owned value must remain valid after the callee exits.

## Runtime failures

Array operations must define behavior for negative/out-of-range indexes, capacity/length overflow, and allocation failure. Those failures must be detected before unsafe C memory operations. Compile-time type/ownership violations are rejected by `ylang check`; dynamic bounds and allocation failures produce a documented nonzero runtime exit status.

## Implementation order

1. Harden scalar borrowing against aliasing and control-flow edge cases.
2. Extend move analysis and replace process-lifetime tracking with deterministic scope cleanup.
3. Add empty-array typing, capacity APIs, and more exhaustive allocation-failure tests.
4. Extend arrays to nested containers only after recursive ownership is sound.
5. Implement modules, typed error results, and the remaining standard-library modules.
6. Update generated Tree-sitter parser artifacts and keep VS Code, Neovim, and LSP in sync.
7. Run sanitizers, compiler fuzzing, and the full Linux/Windows compatibility pass.

## Explicitly deferred

- Nested arrays and user-defined heap types.
- Raw pointers, reference returns, local reference variables, and explicit lifetime annotations.
- A tracing garbage collector and cyclic object graphs.
- Thread-safe shared mutation.
- Rust-equivalent memory-safety claims.

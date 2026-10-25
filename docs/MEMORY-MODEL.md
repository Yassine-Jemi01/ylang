# YLang Memory Model — RFC

Status: experimental, partial implementation on `feature/ownership-memory-v3`. The compiler retains static `move(name)` checks for legacy `string` values and adds a separate `owned string` type with explicit cloning, runtime moves, and generated scope cleanup. This is not a complete Rust-like ownership/borrowing system: borrow syntax/checking and ownership-aware function signatures are still unimplemented.

## Goals

- Keep the default language free of a tracing garbage collector and of periodic GC pauses.
- Free owned heap allocations deterministically when their owner leaves its lifetime.
- Reject use-after-move, dangling borrows, and conflicting mutable borrows at compile time.
- Preserve checked integer arithmetic and array bounds checks unless the compiler proves a check unnecessary.
- Avoid reference counting on ordinary owned values; no cycle collector should be required for the core model.
- Keep the stable 2.0.x language behavior unchanged while the feature is developed behind explicit tests and a release gate.

## Non-goals for the first milestone

- A tracing garbage collector.
- Shared mutable ownership, weak references, or cyclic owning references.
- A global allocator replacement or a promise that every program will be faster than C.
- Silently changing existing `string`, array, or struct behavior before a migration plan exists.
- Treating C code generation as a memory-safety sandbox.

## Proposed ownership categories

The compiler must classify values rather than relying on the C type alone:

1. **Scalar values**: `int`, `float`, `bool`, `char`, and unit-enum values. These are copied by value.
2. **Owned values**: initially dynamic strings, and later owning structs/containers. Every owned value has exactly one owner at a time. Assignment or pass-by-value moves ownership; the old binding becomes unavailable.
3. **Shared immutable borrows**: `&T`. Multiple immutable borrows may coexist while the owner remains alive.
4. **Exclusive mutable borrows**: `&mut T`. Only one exclusive mutable borrow may be active, and no immutable borrow may overlap it.
5. **Non-owning views**: any future slice/string-view type must carry a checked lifetime and may never outlive its backing owner.

The precise surface syntax and which standard-library operations borrow or consume a value must be finalized before implementation. The notation above is a design direction, not currently compilable syntax.

## Lifetime and cleanup rules

- A local owner is destroyed at the end of its lexical scope.
- Returning an owned value transfers ownership to the caller.
- Returning a borrow is allowed only when the type system can prove that the referent outlives the returned borrow; the first implementation may forbid borrowed returns.
- Passing an owned value to an ordinary by-value parameter transfers ownership. A borrow parameter does not consume its argument.
- Every control-flow exit (`return`, `break`, `continue`, and normal block exit) must run exactly the destructors for owners whose scopes are exited.
- Cleanup must be emitted on all paths. The generated C must not rely on non-portable compiler extensions for destructors.
- A resource is released exactly once. Use-after-move and double-release must be rejected or made impossible by generated code.
- If ownership cannot be determined, compilation must fail with an actionable diagnostic rather than guessing.

## Current experimental progress

### Implemented: explicit moves for legacy `string` bindings

The C backend and semantic checker support:

```yl
let string source = string.concat("Y", "Lang");
let string destination = move(source);
print(destination);
```

The checker marks `source` unavailable after the move and reports diagnostic `E2110` if a later read tries to use it. A moved local can be reinitialized with a fresh value. Move state is merged conservatively across `if` branches, loop exits, and exhaustive `match` arms. A self-move assignment is rejected with `E2114`. This initial operation is intentionally limited to named local `string` variables; globals, parameters, arrays, non-string values, and arbitrary expressions are rejected with `E2111`–`E2113` diagnostics.

**Important limitation:** for legacy `string`, `move(name)` remains a static binding operation only; the underlying pointers may alias, so they still use the process-lifetime tracker and are not freed at local scope exit. It is not memory-safe to treat legacy strings as owned values.

The first runtime instrumentation commit adds an opt-in diagnostic:

```sh
YLANG_MEMORY_STATS=1 ./your-program
```

When the generated program exits normally, it writes a line to standard error with the number of currently tracked payload allocations and their requested payload bytes. The counters saturate rather than wrapping. This is diagnostic instrumentation only: it does not reclaim memory earlier, the byte count excludes tracker-node and allocator overhead, and it is not a measurement of resident RAM. Programs using the existing tracker still retain those allocations until `yl_cleanup()`.

The test suite includes an opt-in fixture and verifies that diagnostics do not alter normal program output. The tracked counters cover legacy runtime allocations and are not resident-memory measurements.

### Implemented: separate \`owned string\` locals

This experimental type is distinct from legacy \`string\`:

\`\`\`yl
let owned string source = string.own("hello");
let owned string destination = move(source);
print(destination);
\`\`\`

- \`string.own(value)\` creates an independent heap copy.
- \`move(name)\` transfers an owned pointer and clears the source owner at runtime; static analysis rejects later source reads.
- Owned locals are dropped in reverse declaration order at block exit and on \`return\`, \`break\`, and \`continue\` paths generated by the C backend.
- Reassigning an owned local evaluates its replacement first, then drops the previous value.
- The optional memory diagnostics report owned-string allocations, frees, live allocation count, and live payload bytes.

This first owned type is intentionally local-only. Globals, struct fields, function parameters, and owned return types are rejected until their lifetime/transfer contracts are implemented. The experimental NASM backend rejects owned strings. Borrow syntax, borrow-conflict checking, and complete ownership-aware function signatures are not implemented yet; this is not a general Rust-level memory-safety guarantee.

## Next implementation slices

1. **Borrow checker**: introduce explicit immutable/exclusive borrows, reject conflicts, and prove borrowed values cannot outlive their owners.
2. **Ownership-aware APIs**: define consuming/borrowing contracts for parameters, returns, imports, and standard-library functions.
3. **Broaden owned containers**: support owned fields and containers only after constructors, assignment, cleanup and nested moves have complete rules.
4. **Runtime verification**: run GCC/Clang and ASan/UBSan, plus repeated-allocation tests that assert live owned bytes return to zero.
5. **Performance work**: benchmark equivalent workloads and preserve overflow/bounds checks unless the compiler proves they are redundant.

## Required compile-pass tests

- Moving an owned value makes its source binding unavailable.
- An immutable borrow can be read multiple times while its owner is alive.
- Mutation through `&mut T` is permitted only with exclusive access.
- Returning an owned value keeps it valid in the caller.
- Inner-scope cleanup does not invalidate values that were moved out.
- All valid examples have identical observable behavior under GCC and Clang.

## Required compile-fail tests

- Use after move.
- Borrow after owner destruction / returning a local borrow.
- Two overlapping mutable borrows.
- Mutable and immutable borrows that overlap incompatibly.
- Double move from one binding.
- Moving a value while it is borrowed.
- Invalid use of a moved value on any control-flow branch.

## Performance and memory acceptance criteria

- No regression in stable 2.0.x behavior before the feature is explicitly enabled.
- For a long-running test that repeatedly creates and discards owned buffers, live owned bytes must return to a bounded baseline after each request/scope completes.
- Measure peak RSS, total allocated bytes, total freed bytes, and request throughput; RSS alone is insufficient because allocators may retain freed pages.
- Compare equivalent workloads with equivalent overflow/bounds semantics and the same native compiler flags.
- No performance claim based on one microbenchmark. Use repeated runs, warmups, multiple workload families, and report medians and variance.
- Keep checked arithmetic and bounds checks unless proven redundant by analysis. Any check-elision optimization needs tests that cover both provably safe and potentially overflowing inputs.

## Open design decisions

- Whether current `string` remains a copyable borrowed view and a new `String` type owns bytes, or whether a future major release changes `string` to an owned, move-only value.
- Borrow syntax and inference rules.
- Whether array slices are introduced with the first owned type or later.
- How imported functions expose ownership/borrow information in module signatures.
- How debug diagnostics explain ownership conflicts without producing cascaded errors.

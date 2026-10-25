# YLang Memory Model — RFC

Status: implementation proposal for the `feature/ownership-memory-model` branch. This document does not claim that ownership or borrowing is implemented.

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

The first runtime instrumentation commit adds an opt-in diagnostic:

```sh
YLANG_MEMORY_STATS=1 ./your-program
```

When the generated program exits normally, it writes a line to standard error with the number of currently tracked payload allocations and their requested payload bytes. The counters saturate rather than wrapping. This is diagnostic instrumentation only: it does not reclaim memory earlier, the byte count excludes tracker-node and allocator overhead, and it is not a measurement of resident RAM. Programs using the existing tracker still retain those allocations until `yl_cleanup()`.

The test suite includes an opt-in fixture and checks that enabling the diagnostic does not alter normal program output. Ownership, borrowing, and scope-based release are not implemented by this instrumentation.

## First implementation slice

Implement this in small, reviewable steps:

1. **Specification and regression suite**: codify ownership states, borrowing rules, and rejection tests before changing runtime semantics.
2. **Compiler-side ownership facts**: track declarations, moves, borrow states, and scope exits in semantic analysis. Keep this separate from type checking initially.
3. **A single owned type**: introduce explicit ownership semantics for a new owned string/buffer type, or a deliberately gated string mode. Do not silently make current `string` move-only in a patch release.
4. **Explicit cleanup lowering**: emit cleanup actions for every normal and early scope exit and for owned return values.
5. **Runtime verification**: test allocations/frees under AddressSanitizer and LeakSanitizer where available, plus repeated-request stress tests.
6. **Expand only after parity**: add arrays and structs with owned fields once strings pass the full test matrix.

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

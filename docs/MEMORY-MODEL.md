# YLang Memory Model — RFC

Status: partial implementation on `feature/ownership-memory-model`. The compiler now supports a deliberately narrow, explicit `move(name)` operation for local `string` bindings and rejects a later read of the moved binding. This is not yet a complete ownership/borrowing system: scoped destruction, borrow syntax, borrow checking, and ownership-aware function signatures are still unimplemented.

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

### Implemented: explicit local string moves (initial slice)

The C backend and semantic checker support:

```yl
let string source = string.concat("Y", "Lang");
let string destination = move(source);
print(destination);
```

The checker marks `source` unavailable after the move and reports diagnostic `E2110` if a later read tries to use it. A moved local can be reinitialized with a fresh value. This initial operation is intentionally limited to named local `string` variables; globals, parameters, arrays, non-string values, and arbitrary expressions are rejected with `E2111`–`E2113` diagnostics.

**Important limitation:** `move(name)` currently transfers the binding at compile time and lowers to the same C pointer. It does **not** yet free allocations at scope exit, and the language does not yet prohibit every possible string alias. Do not treat this slice as a Rust-like memory-safety guarantee. Deterministic destruction must not be enabled until borrowing/alias rules and all control-flow exits are handled.

The first runtime instrumentation commit adds an opt-in diagnostic:

```sh
YLANG_MEMORY_STATS=1 ./your-program
```

When the generated program exits normally, it writes a line to standard error with the number of currently tracked payload allocations and their requested payload bytes. The counters saturate rather than wrapping. This is diagnostic instrumentation only: it does not reclaim memory earlier, the byte count excludes tracker-node and allocator overhead, and it is not a measurement of resident RAM. Programs using the existing tracker still retain those allocations until `yl_cleanup()`.

The test suite includes an opt-in fixture and checks that enabling the diagnostic does not alter normal program output. Ownership, borrowing, and scope-based release are not implemented by this instrumentation.

## First implementation slice

Implement this in small, reviewable steps:

1. **Compiler-side ownership facts (in progress)**: the first local-string move state and use-after-move diagnostic are implemented, with pass/fail regression fixtures. Branch-flow joins, loops, borrow states, and imported signatures remain to be implemented.
2. **Borrow checker and explicit type surface**: settle a compatible way to express immutable and mutable borrows, then reject dangling/conflicting borrows and moves while borrowed.
3. **A single fully owned type**: complete the rules for initialization, assignment, parameters, return values, globals, strings builtins, and cloning before adding automatic destruction. Do not silently change current `string` behavior in a patch release.
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

# YLang Memory Model — RFC

Status: partial implementation on `feature/ownership-borrow-checker`. The compiler supports explicit `move(name)`, a limited lexical immutable `borrow(name)` view, and generated cleanup for owned local string bindings and local string-array elements. It is not yet a complete Rust-like memory-safety guarantee: mutable borrows, precise non-lexical lifetimes, signature-level borrow annotations, recursive struct destruction, and automatic cleanup for every possible temporary/string-producing expression remain out of scope.

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
2. **Owned values**: currently, local string bindings and local string-array elements can own tracked allocations. Ordinary string assignment/binding clones an owner to keep compatibility; explicit `move(name)` transfers its pointer and makes the source unavailable. Structs with string fields are not recursively destroyed yet.
3. **Shared immutable borrows**: `&T`. Multiple immutable borrows may coexist while the owner remains alive.
4. **Exclusive mutable borrows**: `&mut T`. Only one exclusive mutable borrow may be active, and no immutable borrow may overlap it.
5. **Non-owning views**: any future slice/string-view type must carry a checked lifetime and may never outlive its backing owner.

The proposed `&T` and `&mut T` surface syntax remains a design direction, not currently compilable syntax. The implemented `borrow(name)` operation only creates a lexical immutable string view.

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

### Implemented: limited immutable string borrows

The compiler accepts an immutable view in a local declaration:

```yl
let string owner = string.concat("Y", "Lang");
{
    let string view = borrow(owner);
    print(view);
}
let string next_owner = move(owner);
```

The view is valid only while its owner is in an enclosing lexical scope. An owner cannot be moved or reassigned while a view is active. Views may be read and copied into another local view; they cannot be reassigned, returned from a function, or passed to a user-defined function because function signatures do not yet express borrowing. The current implementation reports `E2115` for move conflicts, `E2116` for assignment to a view, `E2117` for attempted escape, `E2118` for owner mutation/call-boundary conflicts, and `E2119` for unsupported borrow forms.

This initial view type is immutable and only supports strings. Reborrowing, mutable borrows, field/array views, and non-lexical lifetimes are not implemented. A borrowed view is not separately registered for cleanup and never frees its owner.

### Implemented: explicit local string moves (initial slice)

The C backend and semantic checker support:

```yl
let string source = string.concat("Y", "Lang");
let string destination = move(source);
print(destination);
```

The checker marks `source` unavailable after the move and reports diagnostic `E2110` if a later read tries to use it. A moved local can be reinitialized with a fresh value. Move state is merged conservatively across `if` branches, loop exits, and exhaustive `match` arms. A self-move assignment is rejected with `E2114`, and using `move()` as a nested expression is rejected with `E2120`. This operation is limited to named local `string` variables; globals, parameters, arrays, non-string values, and arbitrary expressions are rejected with `E2111`–`E2113` diagnostics.

**Runtime behavior:** `move(name)` nulls the source pointer and transfers the allocation to the destination. The C backend emits scope frames for blocks and functions, registers owned local strings and local string-array elements, and releases those slots on normal block exit, `return`, `break`, and `continue`. String parameters are cloned into callee-owned bindings; returning a named local owner transfers it to the caller. Ordinary string binding/assignment clones named owners so two slots do not own the same allocation. The tests include a loop/call stress case that checks `live_allocations=0` and `live_bytes=0` after its owners leave scope.

**Remaining limitations:** this is not yet a Rust-level memory-safety guarantee. Strings held inside struct fields are not recursively destroyed, globals live until process shutdown, and some string temporaries that are never bound to an owner slot may remain tracked until program exit. Borrow checking is lexical and limited; it does not support mutable borrows or all lifetime/control-flow cases. Do not use this branch as a hardened safety boundary.

The first runtime instrumentation commit adds an opt-in diagnostic:

```sh
YLANG_MEMORY_STATS=1 ./your-program
```

When the generated program exits normally, it writes cumulative tracked allocations/bytes, explicitly released allocations/bytes, and the live allocations/bytes still in the tracker. Counters saturate rather than wrapping. Byte counts cover requested payload sizes, not tracker-node or allocator overhead, and they are not a measurement of resident RAM. Explicit scope cleanup now reclaims the local-owned values described above; anything left in the tracker is released by the final process cleanup.

The test suite checks that enabling the diagnostic does not change normal program output, and includes a scope/call/loop stress case for owned string cleanup. The instrumentation reports allocator bookkeeping; it does not itself enforce ownership.

## First implementation slice

Implement this in small, reviewable steps:

1. **Ownership-state analysis (in progress)**: local-string moves, basic branch/loop joins, and lexical immutable string borrows are checked. Full path-sensitive borrow dataflow and imported ownership signatures remain to be implemented.
2. **Complete borrow checker**: add `&T` / `&mut T` type rules, mutable exclusivity, reborrowing, precise lifetime joins, and signature-level ownership annotations.
3. **Complete string/temporary coverage**: identify every string-producing built-in and temporary expression, and ensure it is either moved, cloned, borrowed, or released exactly once. Current string binding/assignment behavior remains compatible by cloning named owners.
4. **Recursive destruction**: implement ownership-aware drop routines for structs with string fields and specify global lifetime behavior.
5. **Runtime verification**: run AddressSanitizer/LeakSanitizer and repeated-request stress tests, including early return/break/continue and all supported platforms.
6. **Expand only after parity**: add broader owned containers after strings and structs pass the full test matrix.

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

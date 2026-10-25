# YLang Memory Model — RFC

Status: partial implementation in the experimental C backend. In addition to compile-time `move(name)` checks, the compiler now has an explicit `owned_string` type with move-only binding transfers and generated cleanup at ordinary block exits, function returns, loop breaks, and loop continues. Borrow syntax/checking and full Rust-like lifetime analysis are still unimplemented.

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

### Implemented: experimental `owned_string` and explicit moves

The C backend and semantic checker support:

```yl
let owned_string source = string.own("YLang");
let owned_string joined = owned_string.concat(source, "!");
let owned_string destination = move(joined);
print(source, destination);
```

`owned_string` is a separate experimental type. `string.own(text)` clones text into tracked heap storage; `owned_string.concat/replace/upper/lower` and `io.read_line_owned` / `io.read_file_owned` return owned values. The source of `move(name)` becomes unavailable (diagnostic `E2110`) until reinitialized. Implicit copies of owned bindings are rejected (`E2115`); returning an owned local requires `return move(name)` (`E2116`). Moves are merged conservatively across branches, loops, and exhaustive `match` arms. Self-move assignment is rejected (`E2114`).

Generated C keeps owned slots initialized to null and emits `yl_release` calls on ordinary scope exit and before `return`, `break`, or `continue`. Transferred values are nulled at the source, so cleanup is safe to run on every path. Assigning a new owned value releases the previous value. The runtime release helper only frees pointers registered by YLang's allocation tracker, so string literals and other untracked pointers are not freed accidentally.

The existing `string` type remains a legacy C-string representation; runtime-created legacy strings can still be retained until process exit. `owned_string` is supported only by the C backend for now. Global owned strings, arrays of owned strings, and owned-string struct fields are rejected (`E2118`) until global, array-element, and field cleanup are implemented. User functions that accept `owned_string` take ownership of that argument; call them with `move(name)` or a fresh owned expression.

The separate experimental `borrowed_string` type is permitted only for function parameters. Such parameters are immutable, are passed as non-owning pointers, and cannot be stored in a local/global variable, a struct field, or a return type. They may be forwarded to other borrowed/legacy string parameters or used immediately by string helpers. An `owned_string` temporary is rejected when passed into a borrowed parameter because the temporary would have no named owner to keep it alive for the call. This provides a narrow immutable parameter view, not a general lifetime checker: the language still has no `&mut T`, no returned borrows, and no general alias analysis.

The first runtime instrumentation commit adds an opt-in diagnostic:

```sh
YLANG_MEMORY_STATS=1 ./your-program
```

When the generated program exits normally, it writes a line to standard error with the number of currently tracked payload allocations and their requested payload bytes. The counters saturate rather than wrapping. This is diagnostic instrumentation only: it does not reclaim memory earlier, the byte count excludes tracker-node and allocator overhead, and it is not a measurement of resident RAM. Programs using the existing tracker still retain those allocations until `yl_cleanup()`.

The test suite includes an opt-in fixture and checks that enabling the diagnostic does not alter normal program output. Ownership, borrowing, and scope-based release are not implemented by this instrumentation.

## First implementation slice

Implement this in small, reviewable steps:

1. **Flow-sensitive ownership facts (in progress)**: current checks model explicit moves and merge them conservatively, but complex control-flow should continue to gain regression cases.
2. **Borrow checker**: design explicit immutable and mutable borrow syntax, then reject dangling/conflicting borrows, borrowed moves, and returned local references.
3. **Signature and inter-module ownership contracts**: make ownership transfer vs borrow behavior explicit for all functions and imported signatures.
4. **Cleanup coverage**: expand and verify cleanup for all control-flow edges, then add arrays, structs, globals, and nested owners only when their drop semantics are defined.
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

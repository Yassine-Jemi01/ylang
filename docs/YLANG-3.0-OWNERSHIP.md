# YLang 3.0 Ownership and Borrowing Design

Status: implementation contract proposal for the 3.0 development branch. This document narrows the existing memory-model RFC into an implementable first borrowing milestone. It does not claim that the compiler implements these rules yet.

## 1. Goals

- Keep ordinary values deterministic and avoid a tracing garbage collector.
- Give every owning value exactly one owner.
- Make transfers explicit and reject use-after-move.
- Permit multiple read-only borrows or one exclusive mutable borrow.
- Reject dangling references and conflicting aliases before code generation.
- Keep the first implementation conservative: reject uncertain programs rather than silently extending a borrow's lifetime.
- Preserve the existing `owned_string` behavior while generalizing the semantic model.

## 2. Value categories

| Category | Copy/transfer rule | Initial examples |
|---|---|---|
| Copy | Copy the value | `int`, `float`, `bool`, `char`, unit enums |
| Owned | Move; exactly one live owner | `owned_string`, future owning structs/containers |
| Shared borrow | Non-owning read-only reference | `&T` |
| Mutable borrow | Non-owning exclusive reference | `&mut T` |
| Legacy view | Existing compatibility behavior; not proof of ownership | `string`, current `borrowed_string` |

The legacy `string` and `borrowed_string` types must not be silently reinterpreted as fully checked references. Existing 2.x source behavior remains unchanged.

## 3. Proposed syntax

Use prefix reference types and explicit borrow expressions:

```yl
function length_of(&string text) -> int {
    return string.length(text);
}

function append_mark(&mut owned_string text) -> int {
    text = owned_string.concat(text, "!");
    return 0;
}

function main() -> int {
    let owned_string message = string.own("YLang");
    print(length_of(&message));
    append_mark(&mut message);
    print(message);
    return 0;
}
```

The parser must distinguish a reference type in a parameter/declaration from the unary borrow expression in an initializer. `&mut` is a single mutability-qualified borrow form; `&mut mut T` is invalid. A mutable borrow requires a mutable owner binding.

This syntax is proposed for the first implementation. It is not yet accepted source syntax until parser, semantic, and end-to-end tests pass.

## 4. Borrowing rules — first safe subset

1. A shared borrow may coexist with any number of shared borrows of the same place.
2. A mutable borrow is exclusive: no other shared or mutable borrow of an overlapping place may be live.
3. While a shared borrow is live, the owner cannot be mutated, moved, or mutably borrowed.
4. While a mutable borrow is live, the owner cannot be read through another path, mutated directly, moved, or borrowed again except through a valid reborrow.
5. A borrow cannot outlive its owner. The first implementation uses conservative lexical scopes and may reject some programs that a later non-lexical lifetime analysis could prove safe.
6. Borrowed parameters are valid only for the duration of the call. The callee cannot store them in globals, return them, or place them in an owning aggregate in the first milestone.
7. Borrowed values cannot be returned from functions in the first milestone. Returning an owned value remains an explicit ownership transfer.
8. Moving an owner with any live borrow is an error. Moving a borrowed reference itself does not transfer the underlying owner.
9. Reborrowing `&mut T` as `&T` temporarily suspends mutable access for the reborrow's duration. Reborrowing `&mut T` as `&mut T` is allowed only when the parent mutable reference is not used during the child reborrow.
10. Borrowing a field is supported only after place/projection tracking exists. Until then, borrow the whole struct; overlapping-field disjointness must never be guessed.
11. Raw pointers and unsafe operations are outside this milestone. No pointer operation may bypass the borrow checker.

## 5. Initial lifetime model

- Parameters borrowed by reference live for the duration of the function call.
- Local borrows are bounded by the lexical block containing their binding.
- The referent must be declared in the current or an enclosing scope and must remain initialized for the entire borrow scope.
- A borrow of a temporary is rejected.
- Borrowed returns, closure captures, async suspension across borrows, and storing references in structs/arrays/globals are rejected initially.
- Control-flow joins must conservatively merge borrow state. If one incoming path has a live exclusive borrow and another does not, the merged state remains restricted until the lexical borrow scope ends.

This deliberately favors soundness over accepting every valid Rust-like program. Non-lexical lifetimes can be a later refinement, not a prerequisite for safety.

## 6. Required diagnostics

Reserve stable diagnostic codes in the E2130 range:

- E2130: cannot move a value while it is borrowed.
- E2131: cannot mutate a value while it is immutably borrowed.
- E2132: cannot create a shared borrow while a mutable borrow is active.
- E2133: cannot create a mutable borrow while another borrow is active.
- E2134: borrowed value may outlive its owner.
- E2135: borrowed reference cannot be returned or stored in this context.
- E2136: cannot read an owner while it is mutably borrowed.
- E2137: mutable borrow requires a mutable owner.
- E2138: invalid or unsupported reborrow.
- E2139: borrow of a temporary is not supported.

Diagnostics must include the source location of the conflicting borrow where available and a concise repair hint. Avoid cascaded diagnostics after the first invalid borrow state.

## 7. Compiler architecture

Implement borrowing as a semantic place analysis, not as C pointer tricks:

1. Add reference type metadata without conflating it with existing string types.
2. Represent a place as a root binding plus an optional projection path. Initially only whole-variable places are accepted.
3. Track owner state (initialized/moved) and active loans (shared/exclusive) per function.
4. Introduce explicit borrow and reborrow AST nodes or equivalent typed expression metadata.
5. Run flow-sensitive checks at reads, writes, moves, calls, and scope exits.
6. Merge owner and loan facts at every branch and loop back-edge. Unknown/conflicting states must fail closed.
7. Lower only verified borrows. The C backend may represent a borrow as a pointer to the referent, but generated representation is not the safety mechanism.
8. Extend typed IR with reference type/borrow metadata and verifier invariants before enabling reference operations in non-C backends.
9. Keep deterministic drop insertion separate from borrow validation; cleanup must never invalidate a live borrow.

## 8. Implementation milestones

### M1 — reference syntax and type checking
- Parse `&T` and `&mut T` in function parameter types.
- Parse `&name` and `&mut name` expressions.
- Permit borrowed parameters only; reject returns, storage, and temporaries.
- Add parser and compile-pass/fail fixtures.

### M2 — local borrow state
- Track shared and exclusive loans by root variable.
- Reject move/write/read conflicts.
- Reject a second mutable borrow and shared/mutable overlap.
- Merge loan state conservatively across branches and loops.

### M3 — reborrows and control-flow hardening
- Add shared and mutable reborrows with parent-loan suspension.
- Test early returns, nested scopes, loops, break/continue, and branch joins.
- Fuzz malformed borrow metadata and verify compiler recovery.

### M4 — owned aggregates and projections
- Define drop glue for owning structs and collections.
- Add field-level place projections and disjoint-field rules.
- Add reference-bearing IR metadata and verifier checks.
- Enable only after sanitizer and backend parity tests.

## 9. Acceptance matrix

Compile-pass cases:
- Several shared borrows of one owner.
- A shared borrow passed to a read-only parameter.
- One mutable borrow with mutation through that reference.
- A mutable borrow after all shared borrows leave scope.
- A valid nested reborrow with no parent use during the child loan.
- Moving a value after its borrow scope ends.

Compile-fail cases:
- Move while borrowed.
- Direct write while shared-borrowed.
- Direct read/write while mutably borrowed.
- Two overlapping mutable borrows.
- Shared borrow overlapping a mutable borrow.
- Borrow of a temporary.
- Return or storage of a borrowed parameter.
- Mutable borrow of an immutable binding.
- Conflicting loans merged through either branch of a conditional.
- A borrow that crosses owner scope exit.

Every accepted runtime fixture must produce matching observable behavior with GCC and Clang. Run the existing suite plus strict-warning and ASan/UBSan builds. Do not enable borrowed values in another backend until its representation and verifier rules are implemented.

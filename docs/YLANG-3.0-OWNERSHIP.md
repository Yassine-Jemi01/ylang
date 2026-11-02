# YLang 3.0 Ownership and Borrowing Design

Status: normative design specification for the first 3.0 ownership/borrowing milestone. The compiler now implements a deliberately smaller call-scoped subset of `&T` / `&mut T` in the experimental C backend: direct borrows of named non-global whole-variable owners passed to reference parameters, shared/exclusive conflict checks, and mutation through `&mut`. Local reference bindings, reborrows, projections, flow-sensitive lifetime joins, and general lifetime analysis are not implemented. Sections 4–9 continue to specify the broader target; do not read every listed rule as already implemented.

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

This syntax is accepted for function parameters and direct call arguments in the current limited implementation. Reference declarations, stored/returned borrows, local reference variables, and reborrows remain unsupported. Only named whole-variable owners are accepted; arrays and borrowed_string parameters are not valid reference targets in this slice.

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

## 4A. Ownership and move contract

The following rules define the first milestone independently of backend representation:

- An owned binding is initialized exactly once before it is read. A successful move transfers the value and marks the source binding moved; it does not clone the allocation.
- A read, write, second move, or borrow of a moved binding is rejected until a supported reinitialization restores the binding to the initialized state. A failed assignment does not restore it.
- Copy types are copied at assignment and call boundaries. Owned values are transferred at those boundaries. Borrowed references are non-owning and never trigger destruction of their referent.
- Assignment to an initialized owned binding must first run its old value's drop exactly once, unless the assignment is a supported in-place mutation. The replacement becomes the sole owner only after its value has been successfully produced.
- Returning an owned value transfers it to the caller. Returning a borrowed value is rejected in the first milestone; no implicit lifetime extension is permitted.
- Every owning value has one cleanup obligation. Moving transfers that obligation; dropping consumes it. Cleanup paths must not drop moved values or drop one value twice.
- The compiler must diagnose a use-after-move at the use site and identify the earlier move when source information is available. The diagnostic code remains E2110 for existing owned-string behavior.

## 4B. Shared and exclusive loan contract

A place is initially a whole local binding or parameter. Until projection tracking exists, fields and indices are treated as overlapping with their containing root; the compiler must not infer disjointness.

| Active loan | Shared borrow request | Mutable borrow request | Direct read | Direct write or move |
|---|---|---|---|---|
| None | Allow | Allow only for a mutable owner | Allow | Allow if initialized and mutable as required |
| One or more shared | Allow | Reject (E2133) | Allow | Reject (E2131 for write, E2130 for move) |
| One exclusive | Reject (E2132) | Reject (E2133) | Reject through the owner (E2136) | Reject (E2131/E2130) |

Shared loans may coexist. An exclusive loan excludes every other access path to the same place. Mutation through the exclusive reference itself is allowed; mutation through the owner or another alias is not. A mutable borrow of an immutable binding is rejected with E2137.

## 4C. Lifetime, scope, and alias contract

- A parameter loan begins at function entry and ends when that invocation returns. The callee may use it only during that invocation.
- A local loan begins at its declaration and ends at the end of its lexical block in the first implementation. The owner must outlive the entire block containing the loan.
- A branch or loop merge uses the union of potentially active loans. If any incoming path carries a conflicting loan, the merged place remains restricted until the lexical scope ends.
- Borrowing a temporary, returning a reference, storing a reference in a global/aggregate, capturing it in a closure, or carrying it across an async suspension is rejected in the first milestone.
- Aliases are identified by their root place. Two names that refer to the same root conflict even if the source expressions differ. Unknown alias relationships fail closed.
- A shared reborrow of a shared reference is allowed. A shared reborrow of an exclusive reference suspends exclusive access for the child loan. An exclusive reborrow of an exclusive reference suspends parent access until the child ends. The parent cannot be used while suspended.
- Reborrowing a shared reference as mutable is forbidden. A child loan cannot outlive its parent loan or the owner.

## 4D. Required diagnostic behavior

Diagnostics E2130–E2139 are reserved for the conflicts listed in section 6. Each diagnostic should point at the rejected operation, include the conflicting loan's source span when known, and provide a short repair hint. The compiler should avoid cascaded errors from a single invalid loan transition. Error recovery must restore the semantic loan stack before checking the next independent statement.

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

### M1 — reference syntax and type checking (limited subset implemented)
- Parse `&T` and `&mut T` in function parameter types.
- Parse `&name` and `&mut name` as direct call arguments.
- Keep references call-scoped; stored and returned references remain unsupported.
- Fixtures cover parsing, successful mutable use, and invalid use.

### M2 — call-scoped loan state (partial implementation)
- Track shared and exclusive loans for named non-global whole-variable arguments during a call.
- Reject move/write conflicts, direct owner reads during an exclusive borrow, and mutable borrows of const owners.
- Reject overlapping mutable borrows and shared/mutable overlap in either argument order.
- Mutation through an exclusive reference parameter is supported.
- Global owners are rejected because callees can still access them by name. Local references, branch/loop loan-state joins, reborrows, field/index projections, and general alias analysis remain unimplemented.

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

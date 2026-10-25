# Experimental owned strings

Status: experimental. This feature is being developed behind a compatibility boundary and is not a promise of Rust-level memory safety.

## Why a separate type?

Legacy \`string\` values are represented by C-string pointers and some generated runtime values are retained until process exit. They may alias, so silently freeing them at the end of a local scope would risk dangling pointers. The experimental \`owned string\` type has a separate contract and allocation path.

## Create, clone, move and drop

\`\`\`yl
function main() -> int {
    let owned string source = string.own("YLang");
    let owned string copy = string.own(source);
    let owned string destination = move(source);

    print(copy);
    print(destination);
    return 0;
}
\`\`\`

- \`string.own(value)\` creates a fresh, independent heap allocation from a \`string\` or \`owned string\` value.
- \`move(name)\` transfers a named local owner. The source becomes unavailable to later YLang expressions. At runtime its pointer is cleared so the source's eventual cleanup does not free the transferred allocation.
- Owned locals are freed in reverse declaration order when their block exits. Generated cleanup also handles \`return\`, \`break\`, and \`continue\`.
- Reassigning an owned variable evaluates the new value before freeing the old allocation.
- Owned strings can be printed and passed to the built-in string helpers, which read the bytes or return an ordinary legacy string. Use \`string.own(...)\` when an independent owner is required.

## Borrowed views

\`\`\`yl
function main() -> int {
    let owned string owner = string.own("YLang");
    {
        let &string left = borrow(owner);
        let &string right = borrow(owner);
        print(left, string.length(right));
    }
    {
        let &mut string exclusive = borrow_mut(owner);
        print(exclusive);
    }
    let owned string transferred = move(owner);
    print(transferred);
    return 0;
}
\`\`\`

- Multiple \`&string\` shared views may coexist.
- \`&mut string\` reserves an exclusive borrow of a non-const \`owned string\`; while it is active, the owner cannot be read directly, moved, or replaced.
- Borrows are local bindings, cannot be reassigned, and must be initialized with \`borrow(owner)\` or \`borrow_mut(owner)\`. Nested scopes determine when a binding is no longer active.
- Borrowed views can be read by \`print\`, f-strings, comparisons, and built-in string helpers. \`to_string(view)\` makes a copy so a legacy string value cannot outlive an owned owner accidentally.

The language's current string API is immutable, so \`&mut string\` currently provides exclusivity checking but no byte-mutation operation. Borrowed function parameters/returns, globals, struct fields, reborrowing, and escape analysis across user-defined APIs remain unsupported and are rejected.

## Deliberate limits

The first implementation accepts only initialized local \`owned string\` variables. Globals and struct fields, owned function parameters, and owned return types are rejected because their lifetime/transfer contracts are not implemented yet. Owned strings are supported by the C backend; the experimental NASM backend rejects them.

This is not yet a complete borrow checker for arbitrary program APIs. Avoid storing legacy strings derived from owned values in globals or in longer-lived external state. Do not share the owned pointer with foreign C code that may retain it. This experiment does not make the whole language memory-safe.

## Check the cleanup path

Build and run the regression sample from the repository root:

\`\`\`sh
make
./build/ylang build tests/owned-string.yl -o build/owned-string
YLANG_MEMORY_STATS=1 ./build/owned-string
\`\`\`

The optional diagnostics print the number of owned allocations, frees, remaining allocations, and remaining payload bytes. The regression test checks that all owned allocations in its normal and early-exit paths have been freed. These counters measure tracked payload bytes, not process RSS or allocator overhead.

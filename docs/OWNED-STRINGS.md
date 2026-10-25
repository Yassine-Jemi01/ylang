# Experimental owned strings

Status: experimental. This feature is being developed behind a compatibility boundary and is not a promise of Rust-level memory safety.

## Why a separate type?

Legacy `string` values are represented by C-string pointers and some generated runtime values are retained until process exit. They may alias, so silently freeing them at the end of a local scope would risk dangling pointers. The experimental `owned string` type has a separate contract and allocation path.

## Create, clone, move and drop

```yl
function main() -> int {
    let owned string source = string.own("YLang");
    let owned string copy = string.own(source);
    let owned string destination = move(source);

    print(copy);
    print(destination);
    return 0;
}
```

- `string.own(value)` creates a fresh heap allocation from a `string`, `owned string`, or borrowed string view.
- `move(name)` transfers a named local owner. The source becomes unavailable to later YLang expressions, and its runtime pointer is cleared so cleanup cannot double-free the transferred allocation.
- Owned locals are freed in reverse declaration order when their block exits. Generated cleanup also handles `return`, `break`, and `continue`.
- Reassigning an owned variable evaluates the new value before freeing the old allocation.
- `to_string(view)` copies borrowed data to a legacy string; it does not return an alias that could dangle when the owner is released.

## Borrowed views

```yl
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
    let owned string destination = move(owner);
    print(destination);
    return 0;
}
```

- Multiple `&string` shared views may coexist.
- `&mut string` is an exclusive borrow of a non-const `owned string`. It blocks other borrows, direct owner reads, moves, and owner replacement until its scope ends.
- Borrow bindings cannot be reassigned. Their lexical scope must be nested within the owner's lifetime.
- Borrowed views can be read by `print`, f-strings, comparisons, and built-in string helpers. `string.own(view)` creates an independent owned copy.

The current string API is immutable, so `&mut string` provides exclusivity checking but no operation to mutate bytes yet. Borrowed parameters/returns, globals, struct fields, reborrowing, and escape analysis across user-defined APIs remain unsupported and are rejected.

## Deliberate limits

The first implementation accepts only initialized local `owned string` variables. Globals and struct fields, owned function parameters, and owned return types are rejected until their lifetime/transfer contracts are implemented. Borrow types are also local-only. The experimental NASM backend rejects owned and borrowed strings.

This is not a complete Rust-like memory-safety guarantee. Avoid storing legacy strings derived from owned values in globals or longer-lived external state. Do not share an owned pointer with foreign C code that may retain it.

## Check the cleanup path

Build and run the regression sample from the repository root:

```sh
make
./build/ylang build tests/owned-string.yl -o build/owned-string
YLANG_MEMORY_STATS=1 ./build/owned-string
```

The optional diagnostics print the number of owned allocations, frees, remaining allocations, and remaining payload bytes. Regression tests verify that the owned allocations in the normal and early-exit paths have been freed. These counters measure tracked payload bytes, not process RSS or allocator overhead.

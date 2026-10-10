# YLang Language Specification — 2.0.0-dev

**Status:** development specification. This document describes the current `v2/core` preview, not a stable release guarantee. The stable `main` branch continues to document YLang 1.0.0.

## 1. Language philosophy

YLang aims for four practical defaults:

1. **Readable by design:** explicit types, familiar blocks, and error messages that point to the source.
2. **Ownership is visible:** scalar values copy; strings and arrays move when passed by value or assigned from a named owner; `clone(value)` requests an explicit copy.
3. **Portable programs:** the language runtime is written against the C standard library where possible; the compiler currently emits C17 and delegates native machine-code generation to GCC or Clang.
4. **Useful essentials first:** arrays, checked indexing, input, functions, loops, and a small built-in library come before a large framework or package ecosystem.

These principles are goals for the project, not proof that every current implementation path is memory-safe. YLang 2.0-dev is not yet a Rust-equivalent safety system.

## 2. Compilation model

YLang has a dedicated front-end written in C: lexer, parser, semantic/type checker, diagnostics, and YLang-specific C code generator. `ylang check` validates a source file. `ylang build` validates it, emits temporary C17 code, then invokes GCC or Clang to produce a native executable. `ylang emit-c` writes generated C for inspection.

There is not yet a standalone LLVM or machine-code backend. Generated programs run with the permissions of the user who launches them; compilation is not a sandbox.

## 3. Source form and functions

Statements end with `;`, blocks use `{` and `}`, and line comments start with `//`. Names and keywords are case-sensitive. A single file is compiled as one program. A program must define `function main() -> int` or `function main() -> void`, without parameters.

```ylang
function add(int left, int right) -> int {
    return left + right;
}

function main() -> int {
    print(add(20, 22));
    return 0;
}
```

Function parameters and return types are explicit. Function overloading is not supported. Non-void functions must return on all paths the current control-flow checker can prove.

## 4. Types, variables, and constants

Supported scalar types:

| Type | Meaning |
| --- | --- |
| `int` | Signed 64-bit integer |
| `float` | 64-bit floating-point value |
| `bool` | `true` or `false` |
| `char` | One byte, ASCII-oriented |
| `string` | Immutable, NUL-terminated byte string |
| `void` | Function return type only |

The preview also supports one-dimensional arrays: `int[]`, `float[]`, `bool[]`, `char[]`, and `string[]`. Nested arrays are not supported.

```ylang
let int count = 0;
let const int MAX = 100;
let string title = "YLang";
let int[] scores = [10, 20, 30];
```

A `let type name` binding may be assigned again. `let const type name` cannot be reassigned; const arrays also cannot have elements changed. Local variables must be initialized before they are read.

## 5. Ownership, moving, and cloning

Scalar assignment copies the value. A named `string` or array moved into another binding, returned from a function, or passed to a by-value function parameter leaves its original binding unusable. The compiler reports an error if it detects a later use. Reinitializing a moved binding is allowed.

```ylang
let string first = "YLang";
let string second = first; // moves the value from first
// print(first);            // rejected: value was moved

let string copy = clone(second); // explicit independent string copy
print(copy);
```

`clone(value)` supports strings and one-dimensional arrays. Scalar values already copy by value. `append(array, value)` consumes a mutable local array and returns the extended array value; write the result back to the binding:

```ylang
let int[] values = [1, 2, 3];
values = append(values, 4);
print(values[3]);
```

YLang currently implements a first-pass move checker and scalar-only function borrowing. It is **not a complete borrow checker**: branches and loops use conservative move-state merging, references cannot be stored or returned, and borrowing is currently restricted to `int`, `float`, `bool`, and `char` parameters. Borrowing strings and arrays is not implemented. Cleanup on early control-flow exits still needs more work; runtime allocations are tracked and released at program shutdown as a fallback. Do not treat the preview as memory-safe for hostile or security-critical workloads.

## 6. Arrays

Array literals infer their element type from non-empty values or from the declared array type when context is available. Empty literals require a type context.

```ylang
let int[] numbers = [10, 20, 30];
numbers[1] = 99;
print(numbers[1]);       // 99
print(len(numbers));     // 3

let int[] copy = clone(numbers);
copy[0] = 42;
print(numbers[0]);       // 10
print(copy[0]);          // 42
```

Indexes are zero-based and checked at runtime. Negative or out-of-range indexes terminate with a runtime error rather than making an unchecked array access. All array elements must have exactly the same type; implicit conversions are not performed. Global array initialization is not supported in this preview. Arrays cannot be printed directly; print individual values.

## 7. Conditions and loops

`if`, `else if`, `else`, and `loop()` are supported. The preview adds the familiar three-clause `for` loop:

```ylang
let int sum = 0;
for (let int i = 0; i < 5; i = i + 1) {
    sum = sum + i;
}
print(sum); // 10
```

The initializer must be a scalar `let` declaration or empty. The condition must have type `bool`. `break` and `continue` are only valid inside loops.

## 8. Built-ins and strings

`print(value, ...)` prints values separated by spaces and ends the line. F-strings support interpolating scalar and string expressions, such as `f"Count: {count}"`.

The current built-ins are:

| Built-in | Behavior |
| --- | --- |
| `input()` | Reads one line and returns a string |
| `input_int()` | Reads one line and parses a signed integer |
| `input_float()` | Reads one line and parses a finite float |
| `len(string_or_array)` | Returns byte length for strings or element count for arrays |
| `clone(string_or_array)` | Creates an independent string or array copy |
| `append(array, value)` | Consumes an array value and returns it with one appended element |

Input lines are limited to 1 MiB. Invalid numeric input and unexpected EOF produce a runtime error (status 70). String lengths count bytes, not Unicode scalar values. The source syntax supports a one-byte `char`, not full Unicode character semantics.

## 9. Diagnostics and runtime checks

Compiler diagnostics include an error code, path, line/column, source excerpt, caret, and a hint where available. A failed `check` or `build` does not intentionally produce a successful executable. The runtime checks signed integer overflow and division by zero, array bounds, array-capacity overflow, and allocation errors.

The generated C runtime is part of the trusted implementation. Sanitizers and regression tests help find defects but do not prove that every generated program is free of undefined behavior.

## 10. Current limitations and next milestones

Not yet supported: modules/imports, package management, generics, nested arrays, user-defined structs/classes, exceptions or a typed `Result` error model, async/concurrency, raw pointers, local reference variables/lifetime annotations, comprehensive file/network APIs, and a direct LLVM/native-code backend.

Before a stable 2.x release, the project still needs a stronger ownership/lifetime analysis, deterministic cleanup on every exit path, file I/O APIs, module/import design, additional standard-library tests, and successful Linux plus Windows CI for the complete preview. Tree-sitter, VS Code, and Neovim must remain aligned with the compiler grammar.

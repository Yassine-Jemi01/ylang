# YLang Foreign Function Interface (FFI) — Design

Status: proposed implementation contract for YLang 2.3 development. This document does not claim that external function declarations are implemented.

## 1. Purpose

Provide a small, explicit C ABI boundary so YLang programs can call native libraries, including Rust libraries compiled with `extern "C"`. The first implementation must be sufficient for the Water terminal bridge without exposing arbitrary native memory to ordinary YLang code.

The C backend remains the reference backend. The experimental NASM backend must reject external declarations with a stable diagnostic until it has an independently tested ABI implementation.

## 2. Design principles

- **Explicit boundary:** native calls are declared explicitly; ordinary YLang functions retain their existing semantics.
- **No arbitrary symbol construction:** external symbol names are static declarations, never strings assembled at runtime.
- **No Rust types across the boundary:** only C ABI types may cross the boundary.
- **No implicit ownership transfer:** arguments are borrowed for the duration of the call unless a future API explicitly defines otherwise.
- **No hidden dependencies:** a program that does not use an external declaration must not gain a native library dependency.
- **No shell-based linking:** the driver must pass an argument vector to the native compiler/linker.
- **Fail closed:** unsupported types, calling conventions, and backends produce source-located diagnostics before code generation.

## 3. Proposed source syntax

The initial syntax follows the existing YLang function declaration style:

```ylang
extern "C" function water_terminal_create(int columns, int rows) -> int;

function main() -> int {
    let int terminal = water_terminal_create(80, 24);
    return terminal;
}
```

The external declaration has no YLang body. The symbol name is the declared function name in FFI v1; aliases and custom symbol names are deferred.

## 4. Supported ABI types — FFI v1

| YLang type | C ABI type | Direction |
| --- | --- | --- |
| `int` | `int64_t` | argument / return |
| `float` | `double` | argument / return |
| `bool` | `bool` | argument / return |
| `char` | `char` | argument / return |
| `byte` | `uint8_t` | argument / return |
| `string` | `const char *` | argument only in v1 |
| `void` | `void` | return only |

External string arguments are NUL-free and borrowed only for the duration of the call. The callee must not retain the pointer. External functions must not return allocated strings in v1.

Fixed-size arrays and raw pointers are deliberately excluded from the first milestone. A future buffer API must define pointer/length pairing, mutability, bounds, lifetime, and ownership together. Opaque native resources should initially be represented by integer handles managed by the native bridge, not raw pointers exposed to YLang.

Structs, callbacks, variadic functions, function pointers, C++ ABI, custom calling conventions, and native exceptions are out of scope for FFI v1.

## 5. Linking model

External declarations activate the FFI feature gate. The native build driver may receive explicit link options from a project/build configuration:

- library search directories;
- library names;
- platform-specific system libraries.

All options are passed as individual process arguments. No shell interpolation or command-string execution is permitted. The compiler must not guess library names from source identifiers.

A normal program with no external declarations must build exactly as before and must not require any FFI library.

The initial implementation should support the repository's existing GCC/Clang native driver. Platform-specific support must be tested rather than inferred. The NASM backend rejects FFI v1.

## 6. C code generation

For each external declaration, emit a C prototype using only the approved fixed-width C types. Calls are emitted as direct calls to the declared symbol.

The generated prototype and call must preserve argument order and return type. The semantic checker must reject mismatched arity, argument types, and unsupported return types before C is emitted.

The compiler must not expose arbitrary include directives or arbitrary C source fragments through the FFI syntax.

## 7. Rust bridge contract

Rust libraries used by YLang must export a C ABI:

```rust
#[unsafe(no_mangle)]
pub extern "C" fn water_terminal_create(columns: i64, rows: i64) -> i64 {
    // Return an opaque integer handle, or a documented error value.
    0
}
```

The exact Rust attribute spelling depends on the repository's supported Rust toolchain. This snippet is illustrative, not a committed implementation.

Rust bridge requirements:

- Export only `extern "C"` functions.
- Use `#[repr(C)]` for any future shared structs.
- Do not unwind or panic across the ABI boundary; catch panics at exported entry points and return documented error codes.
- Do not return Rust references, slices, strings, vectors, trait objects, or Rust-owned pointers.
- Define handle validity, invalid-handle behavior, and destruction functions.
- Validate every length and integer conversion at the boundary.
- Keep all Rust allocations and deallocations on the Rust side.

## 8. Diagnostics

The implementation must add stable, source-located diagnostics for at least:

- external declarations with a body;
- unsupported ABI types;
- duplicate external declarations;
- conflicting YLang and external function declarations;
- invalid argument count or argument types;
- external declarations used by the NASM backend;
- invalid or missing linker configuration.

## 9. Required tests

1. Check a valid external declaration without requiring the external library to be installed.
2. Emit C and verify the exact prototype and call signature.
3. Build and run a tiny C shared/static library fixture from a YLang program.
4. Verify that a non-FFI program builds without any extra library flags.
5. Reject unsupported types and malformed declarations with stable diagnostic codes.
6. Reject FFI programs on the NASM backend before assembly generation.
7. Verify that linker arguments containing spaces are passed as single arguments and are never interpreted by a shell.
8. Run the FFI fixture under ASan/UBSan where supported.

## 10. Implementation sequence

- **YP-01a — Contract:** approve this ABI and syntax before compiler changes.
- **YP-01b — Parser and AST:** represent external declarations explicitly.
- **YP-01c — Semantic checking:** validate ABI types, declarations, calls, and backend capability.
- **YP-01d — C generation:** emit prototypes and direct calls.
- **YP-01e — Native driver:** add safe, optional link configuration.
- **YP-01f — Regression suite:** add C fixture, diagnostics, feature-gate, and sanitizer coverage.
- **YP-01g — Rust proof:** compile a minimal Rust `cdylib`/static library and call it from YLang.

The FFI milestone is complete only when the C fixture and Rust proof both pass. Water-specific APIs must not be added to the compiler before this generic boundary is tested.

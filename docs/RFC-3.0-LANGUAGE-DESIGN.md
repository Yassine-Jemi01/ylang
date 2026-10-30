# YLang 3.0 Language RFC: Modern Bindings and Structured Error Handling

**Status:** Partially implemented; remaining language features are design-only until compiler, runtime, and backend tests land  
**Target:** YLang 3.0.x  
**Scope:** Lexer, parser, AST, semantic analysis, C backend, runtime, tests, and editor tooling

## 1. Summary

YLang 3.0 adopts a `let` / `let mut` binding model, deprecates the old type-first `let` declaration syntax while retaining it during the 3.0 migration period, and introduces two distinct error-handling mechanisms:

- Exceptions: `throw` and `try/catch`.
- Explicit recoverable errors: built-in `Result<T, E>`, `Ok`, `Err`, and `?`.

The C backend remains the reference implementation for 3.0. No feature may be accepted by the parser unless semantic analysis and the selected backend either implement it correctly or reject it with a precise diagnostic.

## 2. Binding syntax

### 2.1 Immutable by default

```ylang
let age: int = 16;
let name: string = "YLang";
let enabled: bool = true;
let MAX_USERS: int = 100;
```

A binding declared with `let` is immutable. It must be initialized at declaration.

### 2.2 Mutable bindings

```ylang
let mut age: int = 16;
let mut count: int;

count = 0;
age += 1;
```

A `let mut` binding may be initialized at declaration or later. Definite-assignment analysis must reject every read that is not proven initialized.

### 2.3 Arrays and aggregate mutation

The new type annotation owns the array shape:

```ylang
let scores: int[] = [10, 20, 30];
let mut values: float[] = [1.0, 2.0, 3.0];
values[0] = 5.0;
```

An immutable binding cannot be used to mutate an array element or a struct field. Mutation through a nested field path requires a mutable root binding. This restriction must be enforced consistently for direct assignment, compound assignment, array indexing, and chained struct fields.

### 2.4 Globals

- New immutable globals require an initializer.
- New mutable globals may retain the current type-appropriate default initialization behavior.
- Global mutability must be checked in the same way as local mutability.
- Imported global bindings retain the mutability declared by their defining module.

### 2.5 Function parameters

The existing parameter syntax remains unchanged in 3.0:

```ylang
fn add(int left, int right) -> int {
    return left + right;
}
```

Parameters are immutable inside the callee in the new model. A function that needs a mutable local copy must declare one explicitly. An explicit mutable-parameter syntax is deferred.

## 3. Legacy syntax and migration

The old `function name(...)` declaration keyword remains temporarily accepted and emits warning `W2302`. All new examples and source code should use `fn name(...)`. The old spelling is planned for removal in a later major release, not during the 3.0 compatibility period.

YLang 3.0 must continue accepting the old forms:

```ylang
let int count = 0;             // legacy mutable
let const int LIMIT = 100;     // legacy immutable
let int values[] = [1, 2, 3];  // legacy mutable array
```

Equivalent new forms:

```ylang
let mut count: int = 0;
let LIMIT: int = 100;
let mut values: int[] = [1, 2, 3];
```

Requirements:

1. Preserve the old mutability semantics exactly.
2. Emit a deprecation warning, not an error.
3. Do not silently reinterpret legacy `let Type name` as immutable.
4. Keep source locations and stable warning codes.
5. Add a safe migration path to `ylang fix` only when the declaration can be transformed unambiguously.
6. Do not remove legacy syntax during the 3.0 development cycle; consider removal only in a future major release with an explicit migration plan.

Suggested diagnostic:

```text
warning[W2301]: legacy variable declaration syntax
help: replace 'let int count' with 'let mut count: int'
```

A separate warning may identify legacy `let const Type name` declarations and show the immutable replacement.

## 4. Lexer changes

Current files:
- `include/ylang/lexer.h`
- `src/lexer.c`

Current branch status:
- `TOKEN_MUT`, `TOKEN_COLON`, and `TOKEN_QUESTION` are already present in the lexer.
- `TOKEN_THROW` is not yet present.
- `TOKEN_TRY` and `TOKEN_CATCH` are recognized by the lexer but are not executable language constructs.
- `TOKEN_CONST` remains for legacy declarations.

Do not add syntax highlighting for `?` as a standalone Tree-sitter node until the grammar contains a real postfix propagation rule. The lexer token alone does not make Result propagation available.

Lexer tests must cover token types, ordering, line/column positions, malformed punctuation, and keyword boundaries.

## 5. Parser and grammar changes

Current files:
- `src/parser.c`
- `grammar.js`

New declaration grammar:

```text
immutable_declaration:
  "let" identifier ":" type optional_initializer ";"

mutable_declaration:
  "let" "mut" identifier ":" type optional_initializer ";"

legacy_declaration:
  "let" optional("const") type identifier optional_array_suffix
  optional_initializer ";"
```

The type grammar must support the existing scalar and user-defined types plus array suffixes. Parsing must disambiguate old and new forms without relying on fragile token lookahead beyond the declaration prefix.

For `for` initializers, support both `let mut i: int = 0` and the legacy form, with the same warning policy.

Still to implement in both compiler grammar and Tree-sitter:
- `try { ... } catch (Error error) { ... }`
- `throw expression;`
- Postfix `?`, binding more tightly than binary operators and rejected outside a valid Result context.

Extend match patterns for `Ok(payload)` and `Err(payload)` without weakening exhaustive matching for existing enums.

## 6. AST changes

Current public declarations:
- `include/ylang/compiler.h`

Replace the long-term use of `VarDecl.is_const` with an authoritative mutability enum:

```c
typedef enum {
    BINDING_IMMUTABLE,
    BINDING_MUTABLE
} BindingMutability;
```

Add explicit metadata for:
- Binding mutability.
- Whether legacy declaration syntax was used.
- Whether the declaration requires a deprecation diagnostic.

Add statement kinds for `STMT_TRY` and `STMT_THROW`, an expression kind for Result propagation, and payload-binding information to match arms.

The implementation may temporarily retain `is_const` while migrating internal code, but two conflicting sources of truth must not remain.

## 7. Semantic analysis

Current file: `src/sema.c`

### 7.1 Mutability

All writes must verify that the root binding is mutable. This includes:
- Direct assignment.
- `+=` and increment operations.
- Array element assignment.
- Struct field assignment.
- Chained struct field assignment.

An immutable binding must not become mutable because it is accessed through a pointer-like or aggregate expression. Unsupported mutation forms must be rejected explicitly.

### 7.2 Definite assignment

- Immutable locals require an initializer.
- Mutable locals may be declared without one.
- Reads require proof of initialization on every incoming control-flow path.
- Branches, loops, match arms, try/catch paths, and early exits must merge initialization state conservatively.

### 7.3 Exception checking

The first implementation supports a built-in `Error` exception type and one `catch (Error name)` clause per try statement.

The checker must validate:
- Throw operand type.
- Catch binding type and scope.
- Nested try/catch visibility.
- Unreachable statements after unconditional throw.
- Flow state at normal and exceptional exits.

### 7.4 Result checking

Introduce a compiler-provided generic `Result<T, E>` with `Ok` and `Err` variants. This does not imply general user-defined generics support.

Initially:
- Result types are interned by their `T` and `E` arguments.
- Constructor payloads must match the instantiated type.
- Matching must handle exactly one `Ok` and one `Err` arm.
- Payload bindings receive the corresponding concrete types.
- `?` is valid only in a function returning a compatible Result.
- Error types must match exactly; implicit conversions are deferred.

Owned payload types must remain unsupported until their move/drop behavior is defined and tested.

## 8. Error semantics

### 8.1 Exceptions

```ylang
fn validate_age(int age) -> int {
    if (age < 0) {
        throw Error("Age cannot be negative");
    }
    return age;
}

fn main() -> int {
    try {
        print(validate_age(-1));
    }
    catch (Error error) {
        print(error.message);
    }
    return 0;
}
```

Exceptions are not ordinary return values. They propagate through function calls until a compatible handler is reached.

The initial Error API should expose a stable code and a message. Custom exception inheritance, multiple typed catch clauses, and `finally` are deferred.

### 8.2 Result

```ylang
fn divide(int a, int b) -> Result<int, Error> {
    if (b == 0) {
        return Err(Error("Division by zero"));
    }
    return Ok(a / b);
}
```

### 8.3 Propagation

```ylang
fn read_port() -> Result<int, Error> {
    let parsed: Result<int, Error> = parse_port();
    let port: int = parsed?;
    return Ok(port);
}
```

On `Ok`, `?` extracts the payload. On `Err`, it returns the error from the current function. It must not convert an Err into an exception.

A thrown exception must not silently become a Result error. Any explicit conversion between the two mechanisms requires a separately documented API.

## 9. C backend and runtime

Current files:
- `src/codegen.c`
- `runtime/runtime_template.c`

The current runtime terminates immediately for many failures. Catchable runtime operations must be migrated to a context-aware error-reporting contract.

### 9.1 Execution context

Use an explicit per-execution context passed through generated function calls. A conceptual form is:

```c
typedef struct {
    YLError *pending_error;
} YLExecContext;
```

This context must not be global mutable state. It must support nested handlers and future asynchronous execution contexts.

The generated C function ABI may use a hidden context parameter. The YLang-level function signature remains unchanged.

### 9.2 No unsafe shortcut

Do not implement exception handling using unguarded `longjmp` across generated scopes. Such jumps can bypass cleanup for `owned_string` and other future owned values.

Lower exceptional exits through explicit control-flow and cleanup paths. Before entering a catch block:
1. Run cleanup for every exited protected scope.
2. Transfer or preserve the error payload safely.
3. Clear the handled pending-error state.
4. Enter the catch body.

Uncaught exceptions must print to standard error, run applicable process cleanup, and preserve exit status 70.

### 9.3 Result representation

Use generated tagged representations specialized for each instantiated Result type. Never read an inactive union member. The `?` lowering must run the required cleanup before returning Err.

The generated code must preserve the existing left-to-right evaluation guarantees for calls, operands, initializers, and assignments.

## 10. NASM and other backends

The C backend is the implementation reference for 3.0.

The NASM backend must:
- Support a feature only after correct lowering and tests exist.
- Reject unsupported syntax with a clear backend diagnostic.
- Never silently fall back to the C backend.
- Never claim parity from frontend acceptance alone.

LLVM backend work is governed by the separate LLVM 3.0 RFC.

## 11. Tests and diagnostics

Add frontend tests to `tests/frontend_unit.c` and integration fixtures to `tests/run.sh`.

Required compile-pass coverage:
- Immutable and mutable bindings.
- Mutable arrays and struct fields.
- Legacy syntax and warnings.
- Valid nested try/catch.
- Explicit throw and cross-function propagation.
- Result Ok/Err construction.
- Exhaustive Result matching and payload scopes.
- Valid `?` propagation.

Required compile-fail coverage:
- Reassignment of immutable bindings.
- Mutation through immutable arrays/struct roots.
- Uninitialized immutable declaration.
- Read-before-initialization.
- Invalid throw/catch types.
- Missing or duplicate Result arms.
- Incompatible Result payloads.
- `?` in a non-Result function.
- Incompatible Result error types.
- Unsupported ownership combinations.

Required runtime coverage:
- Caught and uncaught exceptions.
- Nested handlers.
- Runtime helper failures.
- Early return, break, and continue from protected scopes.
- Exception propagation through multiple function calls.
- Owned-value cleanup after exceptions.
- Result success and failure paths.
- GCC/Clang output and exit-status parity.

Run GCC, Clang, AddressSanitizer, and UndefinedBehaviorSanitizer. Test error paths as carefully as successful paths.

## 12. Release gates

YLang 3.0 must not claim these features complete until:
- Both new and legacy declarations have verified semantics.
- Mutability is enforced for every supported assignment target.
- Exceptions can be caught across generated function calls.
- Result construction, matching, and propagation are correct.
- Every tested exceptional exit runs cleanup exactly once.
- Uncaught exceptions retain status 70.
- Unsupported backend combinations fail explicitly.
- Documentation, Tree-sitter, editor grammars, and diagnostics are updated.

## 13. Non-goals

This RFC does not require general user-defined generics, type constraints, exception inheritance, multiple catch clauses, finally, full Rust-style borrow checking, a tracing GC, a new LLVM backend, or full NASM parity.

**End of RFC-3.0-LANGUAGE.**

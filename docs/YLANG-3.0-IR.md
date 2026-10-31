# YLang 3.0 Typed IR — Verifier Contract

Status: standalone typed SSA verifier plus an expanding scalar AST lowering slice. The lowerer supports nested conditionals, scalar SSA merges, `while`, `for`, `loop()`, `break`, `continue`, and direct user-function calls with scalar int/bool parameters and int/bool/void returns. General language lowering and backend integration remain incomplete; LLVM, NASM, and the production C build path do not consume this IR yet.

## Design constraints

- The IR is independent of C syntax and target instruction sets. Direct user-function calls carry a readable callee name, a module-qualified callee symbol, typed argument values, and the checked scalar parameter signature. A function lowered from a module receives a symbol built from its canonical module path plus `::` and its source function name, while unowned AST unit-test functions fall back to their source name. The function verifier checks argument value types and SSA dominance, while `yir_verify_module` resolves the qualified symbol to one unique function and compares its complete scalar signature. This allows same-named functions in different imported modules without ambiguous calls; duplicate symbols are rejected.
- Every produced value has an explicit type and a function-wide unique SSA identifier.
- The first block is the function entry.
- Every block ends in exactly one explicit terminator.
- Lowered instructions retain source line and column locations when available.
- Values may cross blocks only when their definition dominates the use.
- Non-entry blocks may declare typed parameters. Branch edges pass one value per target parameter, in order.
- Typed scalar int and bool function parameters are modeled as entry SSA values. Entry block parameters, other parameter types, external calls, memory operations, and ownership metadata are not modeled yet.

## Current types and operations

Types: `void`, `bool`, `i64`, `f64`, and opaque `ptr`.

Operations currently implemented: `const.i64`, `const.bool`, `add.i64`, `sub.i64`, `mul.i64`, `eq.i64`, `ne.i64`, `lt.i64`, `le.i64`, `gt.i64`, `ge.i64`, `not.bool`, direct typed `call`, `br`, `cond_br`, and `return`.

Branch and conditional-branch terminators may carry typed edge arguments. Those arguments initialize the target block's parameters, providing the SSA merge mechanism for control-flow joins.

## Verifier guarantees

The verifier checks:

- Non-empty functions and blocks, unique block IDs, and valid type tags.
- Exactly one final terminator in each block.
- Existing branch destinations and reachable blocks from the entry.
- Function-wide unique SSA identifiers, including function and block parameters.
- Operand type agreement and definition-before-use within a block.
- Cross-block use only when the defining block dominates the use.
- Block argument count, type, and availability on each incoming edge.
- Return type agreement and the absence of values on void returns.
- Source line and column metadata must be supplied together.

The verifier computes CFG reachability and iterative dominator sets. It rejects unreachable blocks and values that do not dominate a control-flow join.

## Remaining work before frontend integration

This IR is not ready to replace the current AST-to-C path. Full calling-convention and ABI metadata, source spans, floating-point and string operations, memory and aggregate operations, external and indirect calls, module-qualified symbol IDs for same-named functions, ownership/drop metadata, debug locations, and complete frontend lowering for the supported language are still required; current source locations cover lowered instructions, not every IR entity. A first developer-only IR-to-C text emitter now handles the scalar verified subset: i64/bool/void signatures, integer constants and checked arithmetic, integer comparisons, boolean negation, branches with block-parameter copies, direct typed calls, and returns. It uses standalone encoded C identifiers and reproduces checked integer overflow failures for addition, subtraction, and multiplication. It is not yet wired into the production build command; a CLI integration and compile/run differential tests are still needed. Backends must consume the same verified IR before the architecture can be considered integrated. The `emit-mir` CLI command is a developer inspection path only; normal `build` and `emit-c` still use the established AST-to-C pipeline.

## AST lowering scope

`yir_lower_function` currently accepts `int`, `bool`, and `void` functions with zero or more scalar `int` or `bool` parameters. It lowers initialized scalar locals, simple name assignments, nested `if` / `else if` / `else` (including omitted `else`), and scalar SSA value merges at conditional joins. It also lowers `while`, `for`, unconditional `loop()`, `break`, and `continue`, with typed block parameters for loop-carried values. Direct user-function calls are supported when all parameters are scalar `int` or `bool` and the return type is `int`, `bool`, or `void`; this includes nested calls, bool calls in conditions, and void calls as statements. Builtins, imported/indirect calls, external ABI calls, and non-scalar call signatures remain unsupported. Integer expressions are limited to integer literals, negative integer literals, `+`, `-`, and `*`, plus supported direct calls. Conditions support boolean literals, scalar bool names, negation (`!` / `not`), integer comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`), and bool-returning direct calls. Global variables, arrays, compound assignments, uninitialized locals, and other unsupported statements or expressions fail explicitly; they are not silently translated. The resulting function is immediately passed through the function verifier. The `emit-mir` driver also runs the module verifier after every function has been lowered, so direct calls must resolve uniquely within the loaded program. `yir_destroy_function` releases the allocated IR storage.

## Textual IR output

`yir_print_function(FILE *, const YIRFunction *)` writes a stable, human-readable dump of function signatures, typed block parameters, instructions, branch arguments, and available source locations. `ylang emit-mir <file.yl>` runs the existing frontend and semantic checker, lowers every function in the loaded program, verifies each function and the resolved direct-call graph, and then prints the MIR to standard output. It rejects `-o`; unsupported functions produce a source-located error and no partial MIR output. The command currently works only when every function is inside the documented initial scalar lowering subset.

## Build and tests

Run `make ir-unit-test ir-lower-unit-test ir-print-unit-test ir-c-unit-test` or `sh tests/run-ir-unit.sh`. The full `make test` suite also exercises the `emit-mir` CLI. Verifier tests cover arithmetic and return, same-block ordering, cross-block dominance, unreachable blocks, invalid joins, typed block-parameter joins, and module-level direct-call resolution. The initial C emitter tests cover scalar arithmetic, direct calls, conditional branches with block parameters, and explicit rejection of unsupported floating-point signatures. This emitter is currently exercised through the `yir_emit_c_module` API; CLI emission and compile/run parity tests are the next substeps. Lowering tests cover scalar values and assignments, nested conditional joins, omitted `else`, `while`, `for` increment blocks, unconditional loops, loop-carried values, `break` / `continue`, nested direct function calls, boolean-returning calls in conditions, and void calls as statements. Module verifier tests cover successful call resolution, missing callees, signature mismatch, and ambiguous function names, alongside integer and boolean expressions and explicit rejection of unsupported statements. Frontend integration tests parse and semantically check real YLang source before lowering its function AST; CLI coverage checks that control-flow MIR can be emitted from a source file.

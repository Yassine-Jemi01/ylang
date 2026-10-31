# YLang 3.0 Typed IR — Verifier Contract

Status: standalone typed SSA verifier plus an initial scalar AST lowering slice. The lowerer currently handles simple int/bool-return functions with scalar int/bool parameters, integer literals, negative integer literals, scalar local declarations and simple assignments, integer arithmetic, boolean conditions, and a limited if/else return form, plus empty void functions. It does not yet lower general programs, and LLVM, NASM, and C code generation do not consume this IR yet.

## Design constraints

- The IR is independent of C syntax and target instruction sets.
- Every produced value has an explicit type and a function-wide unique SSA identifier.
- The first block is the function entry.
- Every block ends in exactly one explicit terminator.
- Lowered instructions retain source line and column locations when available.
- Values may cross blocks only when their definition dominates the use.
- Non-entry blocks may declare typed parameters. Branch edges pass one value per target parameter, in order.
- Typed scalar int and bool function parameters are modeled as entry SSA values. Entry block parameters, other parameter types, calls, memory operations, and ownership metadata are not modeled yet.

## Current types and operations

Types: `void`, `bool`, `i64`, `f64`, and opaque `ptr`.

Operations currently implemented: `const.i64`, `const.bool`, `add.i64`, `sub.i64`, `mul.i64`, `eq.i64`, `ne.i64`, `lt.i64`, `le.i64`, `gt.i64`, `ge.i64`, `not.bool`, `br`, `cond_br`, and `return`.

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

This IR is not ready to replace the current AST-to-C path. Full calling-convention and ABI metadata, source spans, floating-point and string operations, memory and aggregate operations, calls, ownership/drop metadata, debug locations, and complete frontend lowering for the supported language are still required; current source locations cover lowered instructions, not every IR entity. Backends must consume the same verified IR before the architecture can be considered integrated. The `emit-mir` CLI command is a developer inspection path only; normal `build` and `emit-c` still use the established AST-to-C pipeline.

## AST lowering scope

`yir_lower_function` currently accepts `int`, `bool`, and `void` functions with zero or more scalar `int` or `bool` parameters. It lowers scalar initialized locals, simple name assignments, a return sequence, an empty void body, or one top-level `if/else` with return-only branch paths (void branches may fall through). Nested control flow and loops are not lowered yet. Integer expressions are limited to integer literals, negative integer literals, and `+`, `-`, and `*`. Conditions support boolean literals, boolean negation (`!` / `not`), and integer comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`). Global variables, arrays, compound assignments, uninitialized locals, and unsupported statements or expressions fail explicitly; they are not silently translated. The resulting function is immediately passed through the verifier. `yir_destroy_function` releases the allocated IR storage.

## Textual IR output

`yir_print_function(FILE *, const YIRFunction *)` writes a stable, human-readable dump of function signatures, typed block parameters, instructions, branch arguments, and available source locations. `ylang emit-mir <file.yl>` runs the existing frontend and semantic checker, lowers every function in the loaded program, verifies every result, and then prints the MIR to standard output. It rejects `-o`; unsupported functions produce a source-located error and no partial MIR output. The command currently works only when every function is inside the documented initial scalar lowering subset.

## Build and tests

Run `make ir-unit-test ir-lower-unit-test ir-print-unit-test` or `sh tests/run-ir-unit.sh`. The full `make test` suite also exercises the `emit-mir` CLI. Verifier tests cover arithmetic and return, same-block ordering, cross-block dominance, unreachable blocks, invalid joins, and typed block-parameter joins. Lowering tests cover integer arithmetic returns, scalar locals and assignments, immutable assignment rejection, integer and boolean function parameters, negative literals, empty void functions, if/else returns, boolean literals, boolean negation, integer comparison conditions, and explicit rejection of unsupported bodies. One integration test parses and semantically checks real YLang source before lowering its function AST.

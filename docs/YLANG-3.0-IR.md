# YLang 3.0 Typed IR — Verifier Contract

Status: standalone typed SSA verifier plus an initial scalar AST lowering slice. The lowerer currently handles simple int-return functions with integer literals, unary negation, and +, -, * expressions, plus empty void functions. It does not yet lower general programs, and LLVM, NASM, and C code generation do not consume this IR yet.

## Design constraints

- The IR is independent of C syntax and target instruction sets.
- Every produced value has an explicit type and a function-wide unique SSA identifier.
- The first block is the function entry.
- Every block ends in exactly one explicit terminator.
- Values may cross blocks only when their definition dominates the use.
- Non-entry blocks may declare typed parameters. Branch edges pass one value per target parameter, in order.
- Entry parameters, function parameters, calls, memory operations, and ownership metadata are not modeled yet.

## Current types and operations

Types: `void`, `bool`, `i64`, `f64`, and opaque `ptr`.

Operations currently implemented: `const.i64`, `const.bool`, `add.i64`, `sub.i64`, `mul.i64`, `eq.i64`, `ne.i64`, `lt.i64`, `le.i64`, `gt.i64`, `ge.i64`, `br`, `cond_br`, and `return`.

Branch and conditional-branch terminators may carry typed edge arguments. Those arguments initialize the target block's parameters, providing the SSA merge mechanism for control-flow joins.

## Verifier guarantees

The verifier checks:

- Non-empty functions and blocks, unique block IDs, and valid type tags.
- Exactly one final terminator in each block.
- Existing branch destinations and reachable blocks from the entry.
- Function-wide unique SSA identifiers, including block parameters.
- Operand type agreement and definition-before-use within a block.
- Cross-block use only when the defining block dominates the use.
- Block argument count, type, and availability on each incoming edge.
- Return type agreement and the absence of values on void returns.

The verifier computes CFG reachability and iterative dominator sets. It rejects unreachable blocks and values that do not dominate a control-flow join.

## Remaining work before frontend integration

This IR is not ready to replace the current AST-to-C path. Function signatures and parameters, source spans, floating-point and string operations, memory and aggregate operations, calls, ownership/drop metadata, debug locations, and complete frontend lowering for the supported language are still required. Backends must consume the same verified IR before the architecture can be considered integrated.

## AST lowering scope

`yir_lower_function` currently accepts parameterless `int` and `void` functions. It lowers a single return statement, an empty void body, or a simple `if/else` whose branches each return. Integer expressions are limited to integer literals, negative integer literals, and `+`, `-`, and `*`. Conditions support boolean literals and integer comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`). Unsupported statements and expressions fail explicitly; they are not silently translated. The resulting function is immediately passed through the verifier. `yir_destroy_function` releases the allocated IR storage.

## Build and tests

Run `make ir-unit-test ir-lower-unit-test` or `sh tests/run-ir-unit.sh`. Verifier tests cover arithmetic and return, same-block ordering, cross-block dominance, unreachable blocks, invalid joins, and typed block-parameter joins. Lowering tests cover integer arithmetic returns, negative literals, empty void functions, if/else returns, integer comparison conditions, and explicit rejection of unsupported bodies.

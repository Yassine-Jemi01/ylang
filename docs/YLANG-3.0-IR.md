# YLang 3.0 Typed IR — Initial Contract

Status: initial verifier implementation. The frontend does not lower programs to this IR yet. LLVM, NASM, and C code generation do not consume it yet. This is a deliberately small first slice, not a completed backend architecture.

## Design constraints

- The IR is independent of C syntax and target instruction sets.
- Language semantics remain in the shared lexer, parser, and semantic checker.
- Every operation has an explicit result type and SSA result identifier.
- Basic blocks have stable numeric IDs and end in one explicit terminator.
- Backend lowering must reject operations outside its declared capability set.
- Source spans, function parameters, block parameters, phi nodes, ownership/drop metadata, and debug locations are required before this IR can replace the existing AST-to-C path.

## Initial types

| IR type | Meaning |
|---|---|
| void | No value |
| bool | Boolean control-flow value |
| i64 | Signed 64-bit integer |
| f64 | IEEE-754 binary64 value |
| ptr | Opaque pointer; dereference rules are not defined in this initial slice |

## Initial operations

| Operation | Result | Operands |
|---|---|---|
| const.i64 | i64 | Signed 64-bit immediate |
| add.i64 | i64 | Two earlier i64 values |
| sub.i64 | i64 | Two earlier i64 values |
| mul.i64 | i64 | Two earlier i64 values |
| eq.i64 | bool | Two earlier i64 values |
| br | none | One existing block ID |
| cond_br | none | Earlier bool, two existing block IDs |
| return | none | No value for void, otherwise a value matching the function return type |

## Verifier guarantees and current limits

The verifier checks non-empty functions and blocks, unique block IDs, exactly one final terminator per block, valid branch destinations, unique SSA result IDs within each block, operand definition-before-use and operand/result type agreement within a block, and return type agreement.

The first implementation intentionally restricts operands to values defined earlier in the same block. It does not yet verify control-flow dominance, support values shared across blocks, function parameters, block parameters, phi nodes, floating-point operations, memory operations, calls, or ownership metadata. A block is limited to 4096 result values in this initial verifier.

These limitations are explicit. Do not route real programs through this IR until CFG dominance, block arguments/phi semantics, source locations, and frontend lowering are implemented and tested.

## Build and tests

Run `make ir-unit-test`. The standalone tests cover valid integer arithmetic/return and reject use-before-definition, missing terminators, and mismatched return types.

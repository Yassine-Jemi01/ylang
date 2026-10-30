# RFC-3.0-001: Complete LLVM Backend

**Status:** A backend-neutral IR core, PHI nodes, and control-flow dominance verifier are implemented; AST lowering and LLVM code generation are not implemented  
**Target release:** YLang 3.0.0  
**Primary goal:** A production-quality native backend that lowers checked YLang programs to LLVM IR and emits native object files without generating C as the program's intermediate representation.

## 1. Current repository state

At the time this RFC is prepared:
- The compiler frontend and semantic analysis are written in C17.
- The established backend walks the checked program and emits C; GCC or Clang then produces the executable.
- The experimental NASM backend covers a deliberately restricted Linux x86-64 subset, including fixed-size scalar arrays and pointer-plus-length array parameters for the supported element types.
- A backend-neutral typed IR now has an independent verifier and a scalar AST-lowering prototype on the 3.0 development branch.
- An experimental textual LLVM emitter and explicit `emit-llvm` / `build-llvm` commands have been added on that branch.
- The prototype currently covers scalar integer, float, and bool expressions, calls, control flow, PHIs, checked integer arithmetic, and scalar printing. It is not a complete LLVM backend.
- The repository's main branch does not yet have a complete LLVM code-generation backend.

Installing LLVM or Clang in CI for compiler testing, fuzzing, or host tooling does not mean YLang already generates LLVM IR.

The C backend remains the reference backend while the LLVM backend is developed.

## 2. Goals

The LLVM backend must:
1. Consume the same parsed, resolved, semantically checked program as the C backend.
2. Lower through a backend-neutral typed IR rather than duplicating language semantics.
3. Generate valid LLVM IR and verify it before code generation.
4. Emit native object files for explicitly supported target triples.
5. Link generated objects with the required YLang runtime and system libraries.
6. Preserve observable YLang behavior, including evaluation order, overflow checks, bounds checks, errors, ownership cleanup, and process exit status.
7. Provide optimization levels and usable source-level debug information.
8. Pass differential tests against the C backend for every shared supported feature.
9. Reject unsupported constructs before emission with actionable diagnostics.
10. Avoid making unsupported platform or performance claims.

## 3. Non-goals for the first LLVM release

The first complete LLVM backend does not require:
- A JIT runtime.
- An interactive REPL.
- Self-hosting the YLang compiler.
- Removing the C backend.
- Reimplementing every standard-library routine in LLVM IR.
- Claiming LLVM automatically makes YLang memory-safe.
- Supporting every CPU and operating system from day one.
- A new language semantics specific to LLVM.

## 4. Architecture

### 4.1 One frontend

Keep lexer, parser, module resolution, name resolution, type checking, and source diagnostics shared.

Backend selection must occur after semantic validation. A backend must not implement an alternative type checker.

### 4.2 Typed intermediate representation

Introduce a documented backend-neutral IR between semantic analysis and code generation.

Suggested source layout:

```text
include/ylang/ir.h
src/ir/
  lower.c
  verify.c
  types.c
  dump.c
src/backend/
  backend.h
  backend.c
src/llvm/
  llvm_backend.c
  llvm_types.c
  llvm_expr.c
  llvm_stmt.c
  llvm_debug.c
  llvm_target.c
```

The exact file split may change, but the boundaries must remain explicit.

The IR must represent:
- Scalar and aggregate types.
- Functions, parameters, return types, and globals.
- Typed values and instructions.
- Basic blocks and explicit terminators.
- Branches, loops, match dispatch, and function calls.
- Arrays, structs, enums, and module symbols.
- Source file, line, and column metadata.
- Checked arithmetic and bounds-check operations.
- Ownership transfers and cleanup points.
- Exception edges and Result control flow.
- Runtime and external function declarations.

The typed IR foundation is implemented in `include/ylang/ir.h` and `src/ir.c`. It defines typed SSA values, scalar constants, arithmetic/comparison/print instructions, function calls, functions and basic blocks, and explicit return/branch terminators. Its independent verifier checks value definitions, operand/result types, void calls, print operands, block terminators, return types, branch targets, predecessor-specific PHI inputs, and SSA dominance across reachable control-flow blocks. The scalar AST lowerer is in `src/ir_lower.c`; it currently handles scalar functions, locals, arithmetic, boolean short circuit, if/while/for, calls, and print(int/float/bool). The textual LLVM emitter is in `src/llvm_text.c`. Globals, arrays, strings, aggregates, full runtime, ownership operations, exceptions, Result, debug metadata, and target-machine object emission remain future work.

The IR verifier must reject malformed control flow, mismatched types, invalid terminators, unresolved symbols, and ownership states that cannot be represented safely.

Do not expose LLVM-specific types in the frontend or public language AST.

### 4.3 Lowering strategy

The preferred pipeline is:

```text
.yl source
  -> Lexer
  -> Parser / AST
  -> Module and name resolution
  -> Semantic analysis
  -> Typed YLang IR
  -> IR verification
  -> LLVM IR
  -> LLVM verification
  -> Optimization pipeline
  -> Target machine object
  -> System linker + YLang runtime
  -> Native executable
```

The C backend may continue to consume the checked AST initially. A later migration can lower both backends from the same typed IR.

### 4.4 LLVM API boundary

The current compiler is C17. Before committing to an implementation strategy, build a small technical prototype comparing:
- LLVM's C API.
- A narrow C++ bridge that exposes a stable C ABI to the C compiler.

Evaluate type coverage, target emission, optimization pass access, debug metadata, API maintenance, build complexity, and portability. Choose one approach in a recorded decision before expanding the backend.

Do not scatter LLVM headers or LLVM-owned objects throughout the frontend. Keep LLVM dependencies isolated under `src/llvm/`.

## 5. Backend interface and CLI

Introduce an explicit backend interface with capability reporting.

Proposed CLI:

```sh
ylang check app.yl
ylang build app.yl --backend=c
ylang build app.yl --backend=llvm
ylang emit-llvm app.yl -o app.ll
ylang emit-obj app.yl -o app.o --backend=llvm
ylang build app.yl --backend=llvm --target=x86_64-unknown-linux-gnu
```

Exact flag spelling is subject to CLI consistency review.

Requirements:
- Keep C as the default throughout 3.0 development. Promote LLVM only after the complete 3.0 language and runtime parity gates pass.
- Keep `--backend=c` available as the reference path after LLVM ships.
- Do not silently fall back from LLVM to C.
- `check` must remain backend-independent.
- `emit-llvm` emits inspectable textual LLVM IR for the currently implemented scalar subset.
- `build-llvm` is an experimental Linux x86-64 path that feeds the textual IR to Clang. It is not yet a target-machine object backend.
- `emit-obj` must eventually produce a native object for a supported target.
- Unsupported target/backend combinations must fail before emission.

The current `build-llvm` prototype supports only scalar `int`, `float`, and `bool` computation, scalar function parameters and returns, calls, branches, loops, PHIs, and scalar printing. It rejects globals, arrays, strings, structs, enum printing, ownership, exceptions, Result, and other constructs not yet represented in the IR. Integer overflow, division by zero, and signed division overflow use explicit runtime traps. The prototype uses Clang to compile textual IR and is not a claim of complete LLVM backend coverage.

LLVM should become the default for 3.0 only after all 3.0 acceptance gates pass. The release may retain C as an explicit compatibility backend.

## 6. Target model and linking

Every build must define:
- Target triple.
- CPU and feature selection.
- LLVM data layout.
- Object format.
- Calling convention and ABI.
- Runtime library variant.
- Required system libraries.
- Linker selection.

Start with one fully tested target, preferably `x86_64-unknown-linux-gnu`, then add other targets behind CI and runtime parity gates.

The 3.0 support matrix must be based on actual clean-checkout CI and native runtime tests. Do not infer platform support from LLVM's general target list.

Use LLVM target-machine APIs for object generation. Use a documented system linker or LLD for final executable linking. The compiler must report missing target components, linker errors, and runtime-library mismatches clearly.

Optional facilities such as SDL2 and libcurl must remain optional and must have explicit feature detection and link requirements.

## 7. Runtime ABI

Define a stable internal YLang runtime ABI before the backend grows.

It must cover:
- Checked integer and floating-point operations.
- Bounds checks.
- String representation and allocation.
- Owned-value release and transfer.
- Error creation, propagation, and uncaught-error reporting.
- File and process operations.
- Optional HTTP and GUI facilities.
- Runtime initialization and shutdown.

Do not duplicate runtime semantics in C and LLVM code generators. Prefer shared runtime helpers with documented signatures and tested behavior.

The runtime must not depend on accidental C struct layout across platforms. Use fixed-width types and explicit ABI declarations where appropriate.

For portability, build and package runtime objects/libraries for each claimed target or provide a tested target-specific runtime build path. A system must not need GCC/Clang solely to compile a YLang program when using the complete LLVM backend, unless a clearly documented optional runtime component requires it.

## 8. Language feature coverage

Maintain a machine-readable or test-enforced backend capability matrix.

The LLVM backend must eventually cover every stable 3.0 language feature:
- All documented scalar types and conversions.
- Functions and calls.
- Local and global variables with correct mutability.
- Arrays, bounds checks, and array parameters.
- Structs and enums.
- Exhaustive match and payload variants.
- Modules and named imports.
- Strings and formatted strings.
- Standard library calls.
- Checked arithmetic and runtime errors.
- `Result<T, E>` and propagation.
- `try/catch` and `throw`.
- Ownership transfers and cleanup.
- Debug metadata.
- Optional GUI/HTTP integrations where those are claimed supported.

Experimental features must remain explicitly marked until their implementation and parity tests are complete.

## 9. Optimization pipeline

Expose predictable build modes:
- `-O0`: no optimization, fast builds and debugging.
- `-O1`: basic optimization.
- `-O2`: general release optimization.
- `-O3`: aggressive optimization.
- `-Os` or an equivalent size mode.

Requirements:
- Verify LLVM modules before and after transformations where applicable.
- Keep the optimization pipeline deterministic for a pinned LLVM version.
- Never remove required overflow, bounds, ownership, or error checks unless proven safe.
- Avoid enabling LTO by default until link-time behavior and build reproducibility are tested.
- Publish benchmark methodology and baselines rather than unsupported claims that YLang is faster than other languages.

## 10. Debug information

The backend must generate source-level debug metadata.

Minimum goals:
- Source file and line mapping.
- Function names and call stack.
- Local variable locations where supported.
- Useful debugger behavior at `-O0`.
- DWARF on supported ELF targets.
- Native debug format support for other targets only when validated.

The existing `ylang debug` workflow should be extended to use LLVM-generated objects and the selected platform debugger. Debugger integration must not depend on the generated C source.

## 11. Testing and parity

### IR tests
- Type correctness.
- Valid basic-block terminators.
- Dominance and SSA invariants as applicable.
- Correct branch and loop lowering.
- Source-location preservation.
- Ownership and cleanup invariants.
- Rejection of invalid IR.

### LLVM tests
- LLVM module verification.
- Correct target triple and data layout.
- Object file generation.
- Linking and executable startup.
- Debug metadata.
- Optimization-level behavior.

### Differential tests

For every language feature supported by both C and LLVM backends, compile the same source with both backends and compare:
- Standard output.
- Standard error classification.
- Exit status.
- Runtime error behavior.
- Observable file/process side effects where testable.

Keep the C backend as a behavioral reference, while allowing a documented exception only when a specification explicitly changes and both backends are updated together.

### Toolchain tests

Test the minimum supported LLVM version and the current pinned CI version. Record version detection, missing component diagnostics, and linker behavior.

Run the shared frontend suite, backend-specific suites, fuzzing, and sanitizers on the exact candidate commit.

## 12. Repository changes

Expected implementation areas:

| Area | Files |
|---|---|
| Public IR and backend contract | `include/ylang/ir.h`, `include/ylang/backend.h` |
| IR lowering and verification | `src/ir/` |
| LLVM code generation | `src/llvm/` |
| Driver and CLI | `src/driver.c`, `src/main.c` |
| Build integration | `Makefile`, CI workflows |
| Runtime ABI | `runtime/`, public runtime headers |
| Tests | `tests/ir/`, `tests/llvm/`, parity fixtures |
| Documentation | `docs/architecture.md`, `docs/BACKEND-ROADMAP.md`, LLVM guide |

The implementation must not introduce LLVM dependencies into files that are needed for a compiler build without LLVM unless guarded by backend build configuration.

## 13. Milestones

### L0 — Baseline and API prototype
- Record current C backend behavior and test fixtures.
- Prototype LLVM C API versus a C++ bridge.
- Confirm target-machine object generation and linker invocation.
- Record the chosen LLVM API boundary.

### L1 — Typed YLang IR
- Define IR types, values, blocks, instructions, terminators, and source locations.
- Implement lowering for scalar expressions and simple functions.
- Add IR verifier and textual IR dump.
- Keep the current C backend unchanged.

### L2 — Minimal LLVM executable
- Emit LLVM IR for integer/boolean/character scalars.
- Generate a `main` function and basic printing support.
- Verify IR and emit an object file.
- Link and execute a native binary.

### L3 — Core control flow and functions
- Arithmetic, comparisons, branches, loops, calls, returns.
- Checked integer behavior.
- Differential tests against C.

### L4 — Data and runtime
- Arrays and bounds checks.
- Strings and formatted strings.
- Structs and enums.
- Modules and imports.
- Shared runtime ABI and cleanup.

### L5 — Error and ownership semantics
- Result and propagation.
- Exceptions and handlers.
- Owned values and cleanup on every exit path.
- Sanitizer and error-path tests.

### L6 — Optimization, target and debug support
- Optimization levels.
- Object formats and supported target triples.
- Debug metadata.
- Runtime packaging and system linking.
- CI matrix for every claimed platform.

### L7 — Release hardening
- Full feature parity matrix.
- Performance and binary-size baselines.
- Installation and clean-checkout tests.
- Documentation and migration notes.
- 3.0 release acceptance review.

A milestone is complete only when its tests and documentation are merged. A successful hello-world build is an early proof of concept, not completion of the LLVM backend.

## 14. YLang 3.0 release gates

YLang 3.0 must not be released on the basis of LLVM IR generation alone.

Required:
- [ ] Typed YLang IR is documented, verified, and backend-neutral.
- [ ] LLVM backend builds native executables from a clean checkout.
- [ ] No generated C is used as the YLang program's intermediate representation in LLVM mode.
- [ ] All stable 3.0 language features are supported or explicitly excluded from the 3.0 contract.
- [ ] Unsupported syntax produces clear diagnostics; no silent fallback.
- [ ] C/LLVM differential tests pass for the common feature set.
- [ ] Runtime errors, exceptions, Result, ownership, and cleanup have parity tests.
- [ ] GCC/Clang are not required to compile ordinary programs in LLVM mode, apart from documented optional dependencies.
- [ ] Optimization levels work and preserve semantics.
- [ ] Debug information maps to original YLang source.
- [ ] Each advertised target has native CI, runtime, linker, and installation tests.
- [ ] Sanitizers, fuzz tests, and release acceptance checks pass.
- [ ] Documentation, examples, editor integration, and packages describe actual support.

## 15. Compatibility and migration

- YLang 2.3 keeps C as the default and treats LLVM as experimental until it passes the release gates.
- The 3.0 release may make LLVM the default backend after validation.
- Retain the C backend as an explicit option for compatibility and differential testing where practical.
- Any changed language semantics require a language-specification update and tests for both backends.
- Do not use a major version bump to hide incomplete backend support.

**End of RFC-3.0-001.**

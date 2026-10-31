# YLang 3.0 — Granular Implementation Plan

This is the working task tree for the 3.0 development branch. Each leaf should be small enough to implement, review, and test independently. Status describes the current development branch, not a stable-release guarantee.

Status key: [x] implemented on the branch; [~] implementation exists but its full CI validation is pending; [ ] not started.

## 0. Release and engineering gates

- [x] 0.1 Keep all 3.0 work isolated on `ylang-3.0-dev`.
  - [x] 0.1.1 Keep `main` and stable 2.2.x untouched.
  - [x] 0.1.2 Do not create a 3.0 release or tag automatically.
- [ ] 0.2 Make every leaf task independently verifiable.
  - [ ] 0.2.1 Add a focused unit test for the behavior.
  - [ ] 0.2.2 Add an end-to-end source fixture when the frontend or CLI is involved.
  - [ ] 0.2.3 Run strict-warning, sanitizer, and platform CI applicable to the task.
  - [ ] 0.2.4 Update the relevant design or user documentation.
- [ ] 0.3 Complete final release gates.
  - [ ] 0.3.1 Linux, Windows, and macOS end-to-end suites pass.
  - [ ] 0.3.2 Cross-backend parity, fuzzing, and sanitizer suites pass.
  - [ ] 0.3.3 Migration guide, capability matrix, and benchmark report are complete.
  - [ ] 0.3.4 Owner approves the exact release commit.

## 1. Language design and frontend

### 1.1 Syntax contract
- [x] 1.1.1 Recognize `fn` as the canonical function keyword.
- [x] 1.1.2 Parse `let mut` while preserving legacy mutable `let`.
- [ ] 1.1.3 Specify final immutable/mutable binding rules and migration diagnostics.
- [ ] 1.1.4 Specify expression precedence, evaluation order, and integer overflow behavior.
- [ ] 1.1.5 Add syntax fixtures for every accepted and rejected form.

### 1.2 Type system
- [ ] 1.2.1 Write type identity and unification rules.
- [ ] 1.2.2 Implement generic type parameters for functions.
- [ ] 1.2.3 Implement generic structs and enums.
- [ ] 1.2.4 Implement trait declarations and constraints.
- [ ] 1.2.5 Add diagnostics for inference failures and ambiguous constraints.

### 1.3 Language constructs
- [ ] 1.3.1 Finalize `Option<T>` and `Result<T, E>` semantics.
- [ ] 1.3.2 Implement propagation and exhaustive error handling.
- [ ] 1.3.3 Add tuple and range types.
- [ ] 1.3.4 Add slices and iterator protocol.
- [ ] 1.3.5 Add closures and first-class function values.
- [ ] 1.3.6 Add exhaustive pattern matching and destructuring.

## 2. Typed IR and frontend lowering

### 2.1 IR contract
- [x] 2.1.1 Define typed values, instructions, and function signatures.
- [x] 2.1.2 Define basic blocks and explicit terminators.
- [x] 2.1.3 Define typed block parameters and branch-edge arguments.
- [ ] 2.1.4 Add explicit source spans and debug-location records for all IR entities.
- [ ] 2.1.5 Specify ABI, calling-convention, and target-data metadata.
- [ ] 2.1.6 Specify ownership, move, and drop metadata in the IR.

### 2.2 IR verifier
- [x] 2.2.1 Check value IDs, result types, and operand types.
- [x] 2.2.2 Check block reachability and terminators.
- [x] 2.2.3 Check CFG dominance and same-block definition order.
- [x] 2.2.4 Check typed block-parameter edge arguments.
- [ ] 2.2.5 Add adversarial malformed-IR tests for every opcode and edge shape.
- [ ] 2.2.6 Add verifier fuzzing and resource-limit tests.

### 2.3 AST-to-IR lowering
- [x] 2.3.1 Lower integer and boolean literals and scalar parameters.
- [x] 2.3.2 Lower scalar local declarations and simple assignments.
- [~] 2.3.3 Lower numeric arithmetic and comparison conditions.
  - [x] 2.3.3.1 Lower addition, subtraction, and multiplication.
  - [~] 2.3.3.2 Lower integer division, modulo, and general unary negation with checked runtime semantics.
    - [~] 2.3.3.2.1 Add typed `div.i64`, `mod.i64`, and `neg.i64` IR operations and verifier rules.
    - [~] 2.3.3.2.2 Lower `/`, `%`, and negation of non-literal integer expressions.
    - [~] 2.3.3.2.3 Add stable textual MIR output for the new operations.
    - [~] 2.3.3.2.4 Add checked C helpers for divide/modulo-by-zero and signed overflow edge cases.
    - [~] 2.3.3.2.5 Add runtime tests for divide-by-zero, `INT64_MIN / -1`, negation overflow, and `INT64_MIN % -1 == 0`.
  - [~] 2.3.3.3 Lower the first scalar f64 subset.
    - [~] 2.3.3.3.1 Add finite f64 constants and verifier contracts.
    - [~] 2.3.3.3.2 Lower scalar float parameters, return values, locals, and assignments.
    - [~] 2.3.3.3.3 Lower f64 `+`, `-`, `*`, `/`, unary negation, and matching-type comparisons.
    - [~] 2.3.3.3.4 Support direct calls with scalar float parameters and returns.
    - [~] 2.3.3.3.5 Add verifier, printer, lowerer, C-emitter, and CLI compile/run tests.
    - [~] 2.3.3.3.6 Add an AST-to-C vs IR-to-C parity fixture.
  - [~] 2.3.3.4 Lower short-circuit boolean `and` / `or` using conditional branches and typed merge parameters.
    - [~] 2.3.3.4.1 Skip the right operand of `and` when the left operand is false.
    - [~] 2.3.3.4.2 Skip the right operand of `or` when the left operand is true.
    - [~] 2.3.3.4.3 Preserve left-to-right evaluation for nested logical expressions and calls.
    - [~] 2.3.3.4.4 Compile and run IR-to-C tests where the skipped operand would divide by zero.
- [x] 2.3.4 Lower nested conditionals and merge changed values with block parameters.
- [~] 2.3.5 Lower `while`, `for`, `loop()`, `break`, and `continue`; expand nested-loop and scope edge-case tests.
- [~] 2.3.6 Lower direct user-function calls with scalar signatures.
  - [~] 2.3.6.1 Add typed direct-call opcode metadata and SSA argument validation.
  - [~] 2.3.6.2 Lower nested calls with scalar int/float/bool arguments and int/float/bool/void returns.
  - [~] 2.3.6.3 Print calls in textual MIR and cover the call verifier and lowerer with unit tests.
  - [~] 2.3.6.4 Resolve direct callees against the loaded IR module.
  - [~] 2.3.6.4.1 Verify every call target exists exactly once in the module.
  - [~] 2.3.6.4.2 Compare call arity, parameter types, and return type with the target signature.
  - [~] 2.3.6.4.3 Generate module-qualified IR symbols from canonical module paths; verify same-named functions in different modules resolve independently.
  - [ ] 2.3.6.4.4 Define stable cross-build symbol IDs and backend/linker symbol mangling for imported functions.
  - [~] 2.3.6.5 Lower imported user functions through module-qualified direct calls; imported-module integration coverage is added and awaits CI validation.
  - [~] 2.3.6.5.1 Lower a root module's call to a function imported from another source file.
  - [~] 2.3.6.5.2 Lower transitive imported-function calls across multiple source files.
  - [~] 2.3.6.5.3 Verify same-named functions in separate modules resolve to distinct symbols and execute correctly in the IR-to-C prototype.
  - [ ] 2.3.6.5.4 Define a stable built-in call namespace and lower built-in signatures.
  - [ ] 2.3.6.5.5 Define external function declarations and lower external ABI calls.
  - [ ] 2.3.6.6 Add calling-convention metadata and call-site ABI validation.
- [~] 2.3.7 Complete floating-point semantics beyond the initial finite-f64 lowering slice.
  - [~] 2.3.7.1 Implement finite f64 literals, parameters, locals, arithmetic, comparisons, and direct calls.
  - [ ] 2.3.7.2 Specify NaN, infinities, subnormals, signed zero, and floating-point overflow behavior across backends.
  - [ ] 2.3.7.3 Add explicit int/float conversion operations and diagnostics.
  - [ ] 2.3.7.4 Add float formatting, parsing, and standard-library I/O.
- [ ] 2.3.8 Lower strings, arrays, tuples, structs, enums, and indexing.
- [ ] 2.3.9 Lower imports and cross-module symbol references.
- [ ] 2.3.10 Lower ownership moves, borrows, and deterministic drops.
- [ ] 2.3.11 Match the full stable language feature matrix with explicit unsupported-feature diagnostics.

### 2.3.12 Initial IR optimization
- [~] 2.3.12.1 Add verified constant folding for scalar integer and boolean operations.
  - [~] 2.3.12.1.1 Fold checked add/sub/mul/div/mod/neg only when the result is non-trapping.
  - [~] 2.3.12.1.2 Fold integer comparisons and boolean negation.
  - [~] 2.3.12.1.3 Propagate constants through dominating basic blocks while leaving block parameters unknown.
  - [~] 2.3.12.1.4 Preserve division-by-zero and signed-overflow operations for runtime diagnostics.
  - [~] 2.3.12.1.5 Verify the function/module before and after optimization.
  - [~] 2.3.12.1.6 Add positive, negative, dominance, overflow, and call-preservation tests.
  - [~] 2.3.12.1.7 Fold finite, non-subnormal f64 arithmetic, negation, and comparisons while preserving signed zero.
  - [~] 2.3.12.1.8 Preserve f64 division by either signed zero, non-finite overflow, subnormal results, and possible underflow-to-zero operations.
- [~] 2.3.12.2 Add a developer-only `emit-mir-opt` command to inspect the optimized MIR.
- [ ] 2.3.12.3 Add constant-branch simplification and unreachable-block cleanup.
- [ ] 2.3.12.4 Add dead-code elimination after CFG cleanup.
- [ ] 2.3.12.5 Add a pass manager and ordered optimization pipelines.

### 2.4 IR developer interface
- [x] 2.4.1 Print typed IR in a stable readable format.
- [x] 2.4.2 Add `ylang emit-mir` as a developer inspection command.
- [~] 2.4.3 Add `ylang emit-mir-opt` for the verified initial scalar constant-folding pass.
- [~] 2.4.4 Add machine-readable, versioned JSON IR output for tooling; schema validation and CLI coverage are added, pending CI.
- [ ] 2.4.5 Add IR snapshots and canonical formatting tests.

## 3. Backend integration

### 3.1 Backend contract
- [ ] 3.1.1 Define a backend interface that accepts verified IR.
- [ ] 3.1.2 Define backend capability reporting.
- [ ] 3.1.3 Reject unsupported IR before code generation with source locations.
- [ ] 3.1.4 Define common object, linker, and target configuration APIs.

### 3.2 C reference backend migration
- [ ] 3.2.1 Preserve the current AST-to-C path as a reference during migration.
- [~] 3.2.2 Implement the first standalone IR-to-C emitter slice.
  - [~] 3.2.2.1 Emit i64/f64/bool/void functions with constants, checked integer arithmetic/division/modulo/negation, f64 arithmetic/comparisons, bool/f64 negation, branches, block-parameter copies, direct calls, and returns.
    - [~] 3.2.2.1.1 Encode module-qualified IR symbols as collision-free C identifiers.
    - [~] 3.2.2.1.2 Preserve checked integer and floating-point runtime behavior implemented by the existing runtime.
      - [~] 3.2.2.1.2.1 Preserve `INT64_MIN / -1` and `-INT64_MIN` overflow failures.
      - [~] 3.2.2.1.2.2 Preserve `INT64_MIN % -1 == 0` and modulo-by-zero failure.
      - [~] 3.2.2.1.2.3 Preserve the floating-point division-by-zero diagnostic for both signs of zero.
      - [ ] 3.2.2.1.2.4 Specify and test NaN, infinity, subnormal, signed-zero, and floating-overflow behavior across backends.
    - [~] 3.2.2.1.3 Reject unsupported IR types and opcodes explicitly.
    - [~] 3.2.2.1.4 Add unit tests for integer/f64 arithmetic, direct calls, branch edges, and unsupported signatures.
    - [~] 3.2.2.1.5 Compile and run imported-module calls, including transitive imports and identical function names in separate modules.
- [~] 3.2.3 Add an `emit-ir-c` CLI command that writes verified IR-generated C to a requested file.
  - [~] 3.2.3.1 Register the command and require an explicit `-o` destination.
  - [~] 3.2.3.2 Lower every function and verify the complete module before writing.
  - [~] 3.2.3.3 Generate through a temporary stream and avoid leaving partial output on emission failure.
  - [~] 3.2.3.4 Add emit-ir-c-opt, which runs the verified integer/boolean constant-folding pass before emission.
  - [~] 3.2.3.5 Keep emit-ir-c unchanged as the unoptimized reference path.
- [~] 3.2.4 Add compile/run regressions for generated C integer/f64 arithmetic, checked division/modulo/negation, imported calls, and short-circuit behavior; wait for Linux GCC/Clang CI before marking them validated.
  - [~] 3.2.4.1 Compile and run optimized IR-generated C for safe constant expressions.
  - [~] 3.2.4.2 Confirm optimized IR-generated C preserves divide-by-zero runtime errors.
  - [~] 3.2.4.3 Compare the optimized and unoptimized IR-C outputs with the reference backend.
- [~] 3.2.5 Compare IR-to-C output against the existing AST-to-C backend for matching fixtures; the differential runner is added and awaits CI validation.
  - [~] 3.2.5.1 Build each supported fixture through the reference AST-to-C path.
  - [~] 3.2.5.2 Build and link the same fixture through verified IR-to-C.
  - [~] 3.2.5.3 Compare exit status, stdout, and stderr byte-for-byte across arithmetic, runtime errors, branches, loops, calls, imported modules, short-circuit expressions, and the initial finite-f64 subset.
- [~] 3.2.6 Add branch, loop, short-circuit, imported-call, and f64 differential fixtures; expand sanitizer coverage.
- [ ] 3.2.7 Expand parity feature-by-feature until the supported C subset is covered.
- [ ] 3.2.8 Switch the default build path only after parity and compatibility gates pass.

### 3.3 LLVM backend
- [ ] 3.3.1 Add LLVM toolchain discovery and version checks.
- [ ] 3.3.2 Translate IR types, constants, and arithmetic to LLVM IR.
- [ ] 3.3.3 Translate blocks, branches, and block parameters to LLVM phi nodes.
- [ ] 3.3.4 Add function calls and ABI lowering.
- [ ] 3.3.5 Emit native objects and link executables.
- [ ] 3.3.6 Add debug information and source-level debugging.
- [ ] 3.3.7 Add O0/O1/O2/O3 and size-oriented optimization profiles.
- [ ] 3.3.8 Add Linux, Windows, and macOS target tests.

### 3.4 NASM x86-64 backend
- [ ] 3.4.1 Define target-specific instruction selection from verified IR.
- [ ] 3.4.2 Implement register allocation and spill slots.
- [ ] 3.4.3 Implement stack frames and System V ABI.
- [ ] 3.4.4 Implement Windows x64 ABI.
- [ ] 3.4.5 Implement object generation, relocations, and linking.
- [ ] 3.4.6 Add arithmetic, comparisons, branches, and loops.
- [ ] 3.4.7 Add SIMD only behind runtime/target feature checks.
- [ ] 3.4.8 Run differential parity tests against C and LLVM.

## 4. Memory safety and ownership

### 4.1 Language rules
- [ ] 4.1.1 Specify owned values, moves, and moved-from diagnostics.
- [ ] 4.1.2 Specify shared immutable borrows.
- [ ] 4.1.3 Specify exclusive mutable borrows.
- [ ] 4.1.4 Specify lifetime inference and scope boundaries.
- [ ] 4.1.5 Specify aliasing and reborrow rules.

### 4.2 Compiler implementation
- [ ] 4.2.1 Add ownership state to semantic analysis.
- [ ] 4.2.2 Reject use-after-move.
- [ ] 4.2.3 Reject conflicting mutable/shared borrows.
- [ ] 4.2.4 Add drop scopes and cleanup on return.
- [ ] 4.2.5 Add cleanup on break, continue, and every conditional exit.
- [ ] 4.2.6 Add explicit unsafe blocks and pointer-operation checks.
- [ ] 4.2.7 Add ownership/drop verification in IR.

### 4.3 Runtime memory
- [ ] 4.3.1 Define allocator interfaces.
- [ ] 4.3.2 Add arena and scoped allocators.
- [ ] 4.3.3 Add collection growth and destruction contracts.
- [ ] 4.3.4 Add sanitizer and leak-focused ownership tests.

## 5. Optimization and performance

### 5.1 Measurement
- [ ] 5.1.1 Create reproducible arithmetic and control-flow benchmarks.
- [ ] 5.1.2 Add allocation and memory-footprint benchmarks.
- [ ] 5.1.3 Add compile-time and binary-size benchmarks.
- [ ] 5.1.4 Record CPU, OS, compiler, flags, warmups, and result spread.

### 5.2 Optimization passes
- [ ] 5.2.1 Constant folding.
- [ ] 5.2.2 Constant propagation.
- [ ] 5.2.3 Dead-code elimination.
- [ ] 5.2.4 Copy propagation.
- [ ] 5.2.5 Common-subexpression elimination.
- [ ] 5.2.6 Inlining and devirtualization.
- [ ] 5.2.7 Loop-invariant code motion and induction-variable simplification.
- [ ] 5.2.8 Vectorization with target-feature checks.
- [ ] 5.2.9 LTO and optional PGO.
- [ ] 5.2.10 Validate every optimization with correctness and performance tests.

## 6. Standard library and desktop application development

### 6.1 Core library
- [ ] 6.1.1 Stabilize strings, formatting, conversion, and comparison.
- [ ] 6.1.2 Implement Option, Result, and collection APIs.
- [ ] 6.1.3 Stabilize file, path, process, and environment APIs.
- [ ] 6.1.4 Stabilize time, math, random, and serialization APIs.

### 6.2 Application APIs
- [ ] 6.2.1 Define a stable GUI event loop and ownership model.
- [ ] 6.2.2 Stabilize windows, widgets, layout, and input.
- [ ] 6.2.3 Stabilize image, audio, and graphics resource lifetimes.
- [ ] 6.2.4 Add desktop application examples and packaging tests.
- [ ] 6.2.5 Add a complete small cross-platform desktop application sample.

## 7. Platform support

### 7.1 Windows first-class support
- [ ] 7.1.1 Define Windows toolchain discovery and linker behavior.
- [ ] 7.1.2 Verify filesystem, process, environment, and networking APIs.
- [ ] 7.1.3 Verify GUI dependencies and native packaging.
- [ ] 7.1.4 Run native Windows compiler and executable tests in CI.
- [ ] 7.1.5 Publish supported toolchain setup and troubleshooting docs.

### 7.2 macOS and Linux
- [ ] 7.2.1 Keep Linux x86-64 as the reference CI target.
- [ ] 7.2.2 Verify macOS linker, runtime, GUI, and packaging behavior.
- [ ] 7.2.3 Define ARM64 support only for targets with real CI coverage.

## 8. Developer tools and ecosystem

- [ ] 8.1.1 Stabilize formatter and linter contracts.
- [ ] 8.1.2 Complete LSP diagnostics, navigation, completion, and formatting.
- [ ] 8.1.3 Add debugger integration for supported backends.
- [ ] 8.1.4 Complete project dependency resolution, lockfiles, and offline cache.
- [ ] 8.1.5 Add reproducible builds and package integrity checks.
- [ ] 8.1.6 Complete documentation, examples, migration guide, and release notes.

## Definition of done for every leaf task

A leaf is not marked complete merely because code was committed. It is complete only when:
1. The implementation is limited to the stated behavior.
2. Positive and negative tests cover its contract.
3. Relevant integration tests pass.
4. Diagnostics and failure cleanup are checked.
5. Documentation is updated where user-visible behavior changes.
6. Applicable CI jobs pass; otherwise the task stays marked as validation-pending.

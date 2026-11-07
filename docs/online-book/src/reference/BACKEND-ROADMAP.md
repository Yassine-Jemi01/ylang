# Backend strategy and roadmap

## Current state

- YLang's compiler front end and type checker are written in C17.
- The default `ylang build` backend emits C, then invokes GCC or Clang to produce an executable. This is the broadly supported path for the beta.
- The experimental NASM backend emits Linux x86-64 assembly for a restricted subset and links with NASM plus `ld`. It does not support every feature accepted by the C backend.
- Programs requiring the optional SDL2 runtime need the documented native dependencies; the NASM backend rejects GUI calls explicitly.

This means **YLang programs do not yet have a complete, general-purpose native backend independent of GCC/Clang**. The NASM subset is not a drop-in replacement for the C backend.

## Design principles

1. **One front end.** Parsing, name resolution, type checking, and diagnostics stay shared. Do not implement language semantics separately inside each backend.
2. **Typed intermediate representation first.** Before expanding the NASM subset or starting another backend, introduce a documented, backend-neutral representation that preserves resolved types, control flow, imports, and source locations.
3. **Declare capabilities.** Each backend must have a tested capability matrix. Unsupported language constructs must cause explicit diagnostics before emission; never silently emit a different behavior or fall back without telling the user.
4. **C remains the reference path for broad coverage.** The C backend stays the default until an alternative covers the required language subset and passes the same behavior tests.
5. **Differential testing is mandatory.** For every construct supported by more than one backend, compile the same source with each backend and compare stdout, stderr/runtime-error classification, and exit status. Keep small regression fixtures for arithmetic, control flow, enum matching, and imports.
6. **Fuzz the shared front end.** Keep lexer/parser unit tests and run the libFuzzer harness under AddressSanitizer and UndefinedBehaviorSanitizer in CI where toolchain support is available.

## Proposed milestones

### B0 — 2.0 beta

- Keep C/GCC/Clang as the default supported build path.
- Keep NASM explicitly experimental and limited to its documented Linux x86-64 subset.
- Run differential parity tests on the common subset; report skips on unsupported platforms instead of implying parity was tested.

### B1 — Backend-neutral typed IR

- Specify expression, statement, type, control-flow, module/import, and source-location representations.
- Add IR invariants and tests independent of any target backend.
- Move code generation gradually behind a backend interface without duplicating semantic checks.

### B2 — Native backend proof of concept

Evaluate two options with a small, measurable prototype:

- **LLVM IR/object output**, for mature optimization and target support, while explicitly accounting for LLVM toolchain size and deployment costs.
- **Direct machine-code or assembly generation**, if a small dependency footprint is more important, with target-specific code isolated from the language semantics.

The decision should be based on executable correctness, portability, debug information, compile time, binary size, maintenance cost, and cross-backend test coverage—not on a target date or a single benchmark.

### B3 — Expand only behind parity gates

- Extend a backend only when the IR and tests express the feature fully.
- Require parity fixtures for the feature's supported subset and compile-fail diagnostics for unsupported combinations.
- Keep runtime/standard-library dependencies explicit and versioned.

### B4 — Reassess the GCC/Clang requirement

Only advertise a general independent backend after it can build and run the supported language suite on every claimed target from a clean checkout. Building YLang's compiler itself may still require a C toolchain unless a separate bootstrap path is implemented.

## Out of scope

This is a roadmap, not a promise that LLVM, a direct machine-code backend, Windows support, or a self-hosting compiler will ship in the 2.0 beta. Do not delay the beta for those future milestones, but do not imply that the current NASM subset already removes the C compiler requirement for general YLang programs.

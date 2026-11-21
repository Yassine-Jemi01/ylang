# YLang 2.4 — General-Purpose Capability Expansion

**Status:** development roadmap  
**Base:** YLang 2.3 development line  
**Goal:** remove artificial language and toolchain limitations that prevent users from implementing real-world systems, application, game, networking, and tooling projects in YLang.

This roadmap is a capability target, not a claim that every project can be built without platform, hardware, algorithmic, or ecosystem constraints. YLang must expose the capabilities needed to implement such projects directly, with documented safety rules and tested platform support.

## Product principles

1. **No arbitrary language ceilings.** If a capability is needed for systems programming or application development, provide a principled language or standard-library path for it.
2. **Safe by default, explicit when low-level.** High-level code should remain approachable. Unsafe memory and ABI operations must be explicit, scoped, and auditable—not silently unavailable.
3. **No fake support.** A feature is not complete because it parses or has a wrapper. It needs semantic checks, code generation, runtime behavior, diagnostics, tests, documentation, and supported-backend coverage.
4. **Backends must be honest.** Each backend advertises its capabilities and rejects unsupported constructs before code generation.
5. **Compatibility is deliberate.** Preserve the documented 2.3 stable core. Breaking syntax or ABI changes require migration notes and explicit release gates.
6. **Native interoperability is a first-class requirement.** C ABI interoperability must not require embedding arbitrary C source or invoking a shell.

## Capability workstreams

### P0 — Low-level foundations

- Fixed-width signed and unsigned integer types: int8, int16, int32, int64, uint8, uint16, uint32, and uint64.
- Explicit bitwise operators: &, |, ^, ~, <<, and >>, with defined precedence and shift behavior.
- Checked conversions and clearly specified overflow behavior for every integer type.
- Binary-safe byte values and buffers, including pointer/length boundaries where required.
- Compiler tests for integer literals, min/max values, casts, arithmetic, shifts, and generated C portability.

### P0 — Native interoperability

- Implement the existing restricted C ABI contract in docs/FFI-SPEC.md.
- Add safe project-level linker configuration passed as an argument vector, never through a shell.
- Prove calls to a small C fixture and a Rust extern "C" library.
- Keep native pointers opaque until a complete pointer, lifetime, mutability, and ownership model is implemented.
- Make unsupported ABI types and backend combinations fail with source-located diagnostics.

### P1 — Memory and data model

- Finalize a coherent ownership and borrowing model based on docs/MEMORY-MODEL.md.
- Add explicit references and pointers only with defined nullability, mutability, lifetime, aliasing, and dereference rules.
- Add slices and dynamically sized arrays with checked indexing and explicit ownership.
- Support nested arrays/collections and returning or assigning aggregate values.
- Implement deterministic destruction for owned values across all control-flow exits.

### P1 — Generic programming and reusable data structures

- Add type parameters for functions and data types.
- Define monomorphization or another concrete implementation strategy before exposing generics.
- Add generic standard-library collections: dynamic arrays/lists, maps, sets, queues, and optional values.
- Preserve actionable diagnostics for inference, constraints, and type mismatches.

### P1 — Systems and application capabilities

- Complete filesystem, process, environment, sockets, and platform APIs.
- Add threads, atomics, mutexes, and condition variables with explicit memory-order semantics.
- Define general asynchronous execution separately from the existing HTTP-task API.
- Add callbacks and function pointers only with explicit ABI and lifetime rules.
- Make platform support measurable across Linux, macOS, and Windows.

### P2 — Compiler and backend foundations

- Introduce a backend-neutral typed intermediate representation.
- Make semantic analysis independent from C-specific assumptions.
- Expand the native backend strategy only after the IR and feature contracts are tested.
- Maintain C backend parity while experimental backends evolve.
- Add compile-time resource limits and diagnostics for pathological source programs.

## Initial implementation order

1. Establish this branch and freeze the 2.4 capability contract.
2. Audit the current lexer, parser, semantic checker, AST, C generator, NASM feature gates, and tests for the P0 integer/bitwise work.
3. Add fixed-width integer types and bitwise operators in a minimal vertical slice: lexer → parser → AST → semantic analysis → C codegen → runtime/diagnostic tests.
4. Implement the already documented FFI v1 contract as a separate vertical slice, including linker configuration and C/Rust proof fixtures.
5. Expand arrays and memory semantics only after the required representation and lifetime rules are approved by tests.
6. Implement generics and collections after the type-system design is concrete.
7. Publish a feature/backend/platform matrix and require every release claim to match automated coverage.

## Definition of done

A capability is complete only when all applicable items pass:

- Syntax and type rules are documented.
- Positive and negative compiler tests exist.
- Runtime and boundary tests cover edge cases.
- Generated code is inspected and compiled with GCC and Clang.
- Sanitizer coverage is added where applicable.
- Every backend either passes the feature tests or rejects the feature before emission with a stable diagnostic.
- Linux/macOS/Windows support is stated only for platforms actually tested.
- The online book and examples match the implementation.

## Explicit non-goals

- Claiming that one release can remove every engineering limitation.
- Adding syntax without a coherent runtime and ABI representation.
- Exposing arbitrary native pointers as integers and calling that FFI.
- Claiming memory safety, sandboxing, or Rust-level guarantees without implementing and validating them.
- Sacrificing the stable 2.3 compatibility contract to rush experimental features.

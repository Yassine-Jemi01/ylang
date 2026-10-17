# YLang 2.0 — Language and Ecosystem Design

Status: proposal, not an implementation guarantee.

This document defines the order in which YLang 2.0 should grow. The guiding rule is to land small, testable language changes rather than introducing syntax that the semantic analyzer and backends cannot enforce.

## Goals

- Keep YLang readable and approachable while making invalid states harder to express.
- Make compiler diagnostics and behavior consistent across GCC/Clang C builds and the experimental NASM backend.
- Define type, initialization, ownership, and error-handling semantics before exposing pointers or object-oriented features.
- Ship reproducible compiler releases, a package format, documentation, and a minimal official website.

## Non-goals for the first 2.0 milestone

- Claiming Rust-level memory safety before ownership and lifetime rules are implemented and tested.
- Supporting every feature in every backend at once. Unsupported features must fail with a diagnostic rather than silently miscompile.
- A public package registry before dependency integrity, version resolution, and lockfile behavior are specified.
- Class inheritance, reflection, async/await, or JIT compilation in the first milestone.

## Backend strategy

The C backend remains the default for broad language coverage; it emits C and invokes GCC/Clang. The NASM backend is experimental, restricted to Linux x86-64, and must reject unsupported constructs. YLang does not yet have a complete general-purpose native backend independent of GCC/Clang. The plan is to define a shared typed intermediate representation, keep backend capabilities explicit, and require differential output/exit-status tests for the subset supported by multiple backends. A future proof of concept may evaluate LLVM IR or direct machine-code generation; neither is promised for 2.0.0-beta.1. See [BACKEND-ROADMAP.md](BACKEND-ROADMAP.md).

## Milestone 0 — Compiler correctness

Before new type syntax:

1. Make definite-initialization analysis consistent for assignments, compound assignments, increment/decrement, branches, and loops.
2. Add regression tests for each diagnostic and valid programs near those boundaries.
3. Test the C backend with GCC and Clang; run AddressSanitizer and UndefinedBehaviorSanitizer in CI where supported.
4. Keep the NASM backend explicitly experimental until its supported-language matrix and parity tests are clear.
5. Run generated executables and compare output and exit status; a successful build alone does not prove runtime correctness.

Acceptance criteria:
- Each known bug has a regression test.
- CI reports the compiler, target OS/architecture, and sanitizer result.
- No feature is marked stable only because it parses.

## Milestone 1 — Value types: struct and enum

Start with value types, not heap-allocated classes.

Current experimental subset (available on the development branch; not yet a stable 2.0 guarantee):

~~~yl
struct Point {
    float x;
    float y;
}

function distance_squared(Point point) -> float {
    return point.x * point.x + point.y * point.y;
}

function main() -> int {
    let Point point = Point(3.0, 4.0);
    print(distance_squared(point));
    return 0;
}
~~~

Unit enums and exhaustive `match` statements are also implemented experimentally. See [STRUCTS.md](STRUCTS.md) and [ENUMS.md](ENUMS.md) for the tested subset and explicit limitations.

Required rules:
- A struct declaration has a stable, compile-time-known field layout.
- Every field must be initialized before an instance can be read.
- Field reads and writes are type-checked; unknown fields produce a dedicated diagnostic.
- Duplicate fields and duplicate type declarations are rejected.
- Copy/value semantics must be documented before structs can be passed, returned, or nested.
- Recursive by-value fields are rejected unless a later indirection type makes their size finite.
- Structs must not be accepted by the NASM backend until layout and field operations are implemented there.

Then add enums with exhaustive matching. Do not add pattern matching syntax until exhaustiveness and unreachable-case rules are specified.

## Milestone 2 — Explicit absence and errors

**Not implemented yet.** Introduce `Option<T>` and `Result<T, E>` only after the compiler has a reliable generic/type representation. The current enum implementation has unit variants only and cannot carry payload values.

Illustrative future syntax (not currently guaranteed to compile):

~~~yl
let result: Result<int, ParseError> = string.parse_int(text);
match result {
    Ok(value) => print(value),
    Err(error) => print(error.message),
}
~~~

Rules:
- Missing values must not be represented by undocumented magic integers or null strings.
- Option&lt;T&gt; must force callers to handle the absent case, using matching or a documented unwrap API.
- Result&lt;T, E&gt; must distinguish success from failure and preserve the error value.
- Exhaustiveness checks must reject missing cases.
- Generic instantiation and diagnostics need tests for nested types and invalid type arguments.

## Milestone 3 — Memory model before pointers

Do not add raw pointer syntax before deciding who owns allocations and how long references remain valid.

Proposed order:
1. Define value versus owned heap types.
2. Define deterministic cleanup semantics and behavior on early return.
3. Add immutable and mutable references with explicit aliasing rules.
4. Reject references that can outlive their referent.
5. Add raw pointers only for low-level interoperability, behind an explicit unsafe boundary.
6. Provide checked allocation and null handling at the safe API boundary.

Safety requirements:
- No read of uninitialized storage.
- No safe use-after-free or dangling reference.
- No safe dereference of null.
- No unchecked pointer arithmetic in safe code.
- Bounds checks remain in place for safe indexing.
- FFI and unsafe operations must be visibly marked and documented.

A runtime tracker that frees allocations at process exit is not a substitute for an ownership model. A borrow checker is a substantial feature and should be implemented incrementally with compile-fail tests.

## Milestone 4 — Object-oriented features

Add methods only after structs and field access are stable.

Preferred initial model:
- Methods associated with a named type.
- Explicit receiver semantics (self by value or by reference).
- Encapsulation through access modifiers with documented defaults.
- Interfaces/traits for behavior shared across types.
- Composition preferred over inheritance initially.

Class/reference semantics and inheritance are separate design decisions. Do not silently make all structs heap objects or add inheritance without defining identity, initialization order, dispatch, destruction, and cycle behavior.

## Milestone 5 — Generics and standard library

Generic types and functions must specify type inference, constraints, monomorphization/code generation, and error messages. Start with a small set of constraints rather than unrestricted templates.

Standard-library priorities:
- Option/Result and string parsing.
- File and path APIs with explicit error handling.
- Collections with clear ownership and bounds behavior.
- Tests and documentation for every public API.
- Platform-specific APIs isolated behind portable interfaces.

## Milestone 6 — Ecosystem and package manager

Initial project manifest proposal:

~~~toml
[package]
name = "example"
version = "0.1.0"
ylang = ">=2.0.0"

[dependencies]
# package = "1.2.3"
~~~

The exact schema must be finalized before implementation. The package manager should eventually provide:
- ypm init, add, remove, update, build, and test commands.
- A manifest and committed lockfile with exact resolved versions.
- Integrity hashes for downloaded archives.
- Dependency source allowlisting and safe archive extraction (reject path traversal and unexpected symlinks).
- Deterministic resolution, clear offline behavior, and useful conflict diagnostics.
- Local path dependencies first; pinned Git dependencies next; a central registry only after the trust and publishing model exists.

## Milestone 7 — Releases and website

Release artifacts should be produced by CI, not manually compiled on one developer's machine.

Initial target matrix:
- Linux x86_64.
- Linux aarch64 only after runner/toolchain coverage is confirmed.
- Windows x86_64.
- macOS arm64 and x86_64 where supported by the compiler and CI runners.

Each target must build, run its test suite, and publish checksums. Do not advertise a target until CI has passed on that target.

Minimal website pages:
- Home: what YLang is and a small code sample.
- Documentation: installation, language guide, standard library, diagnostics.
- Downloads: links to actual release artifacts and checksums.
- Packages: package-manager instructions once it exists.
- Playground: only if an isolated, resource-limited execution service exists; never execute arbitrary submitted code directly on a privileged host.

The site should consume release metadata or a generated manifest so download links do not need to be edited by hand for every release.

## GUI and 2D game development

The optional SDL2 runtime and a minimal `gui.*` namespace are implemented and callable from `.yl`. The API currently covers initialization, window creation, event polling, clearing, rectangle drawing, presentation, last-error reporting, and shutdown/close. Type-checking and NASM rejection tests exist; the runtime integration is optional so ordinary non-GUI programs do not require SDL2. See [GUI-API-SPEC.md](GUI-API-SPEC.md) for the current contract.

The experimental `stdlib/game2d.yl` module wraps window/frame/drawing operations, keyboard state, mouse position/buttons, millisecond timing, delta time, rectangle overlap, and point-in-rectangle helpers. See [game2d.md](game2d.md). It is a foundation, not a Pygame replacement.

Remaining game-library work:
1. Add a reusable game-loop abstraction with a documented frame/update contract.
2. Add sprites/textures and integration tests using SDL's dummy video driver where available.
3. Add audio, richer widgets, physics, or 3D only after the minimal 2D API is stable.

The NASM backend should continue to reject unsupported GUI/game calls with an explicit diagnostic. GUI and game features are not complete until Linux/macOS CI, missing-dependency diagnostics, and runtime tests pass.
 
## CI and release gates

Every pull request:
- Builds with GCC and Clang on supported Linux runners.
- Runs unit/integration tests and verifies expected output and exit codes.
- Runs sanitizers where available.
- Runs compile-fail tests and checks stable diagnostic codes.
- Tests NASM only on supported Linux x86_64 runners with NASM and binutils installed.
- Checks formatting and generated artifacts.

Every release:
- Repeats the test matrix on the exact tagged commit.
- Publishes checksums and release notes.
- Documents backend and platform limitations.
- Does not call a feature stable if its acceptance tests are missing.

## Reference designs

These documents are references for principles, not specifications to copy wholesale:

- Swift memory safety and definite initialization: https://docs.swift.org/latest/documentation/the-swift-programming-language/memorysafety/
- Swift type initialization rules: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/initialization/
- Rust references and borrowing: https://doc.rust-lang.org/core/primitive.reference.html
- Python packaging manifest specification: https://packaging.python.org/en/latest/specifications/declaring-project-metadata/

## Decision log

- Keep the current C backend as the stable baseline while experimental NASM support is incomplete.
- Implement struct as the first aggregate type; do not conflate structs with classes.
- Specify ownership and lifetime rules before safe pointers.
- Prefer explicit errors (Result) over hidden failure states.
- Treat the website, package registry, and compiler binaries as release outputs that must reflect verified builds.

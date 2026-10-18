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

## Milestone 4 — Object-oriented features (deferred; not part of 2.0)

OOP is intentionally deferred. Do not add methods, classes, inheritance, or receiver semantics to the 2.0 release work; focus on Windows portability, YSPM system-package integration, and the playable minimal Game2D sample.

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

## Milestone 6 — YSPM Linux system-package integration (required for 2.0)

YSPM (https://github.com/Yassine-Jemi01/yspm) is a Linux system package manager that installs native system packages in the .yspkg format. It is not a package manager for project-local .yl module dependencies, so YLang must not pretend that YSPM resolves source imports or replaces YLANG_PATH.

YLang will provide a release packaging script at packaging/yspm/build-package.sh. It stages the compiler, source-level standard library, optional GUI runtime sources and public header, license, and available documentation, then asks the YSPM CLI to produce a native Linux x86-64 package. CI builds and inspects the package from the exact candidate commit. The beta.2 release workflow builds and attaches the generated .yspkg to the prerelease from the exact tested commit; already-published tags are never overwritten.

The 2.0 release gate requires:
- The package builds from the exact tested commit using a pinned YSPM source revision.
- CI verifies metadata, ABI/architecture, packaged compiler path, standard-library files, and that the packaged compiler starts.
- The release pipeline attaches the package artifact to the new prerelease only after all required gates pass.
- Documentation clearly says that YSPM manages Linux system packages; it is not a language-level module manager.
- The package documents external build prerequisites (GCC or Clang, plus SDL2 development dependencies and pkg-config for GUI/game builds).

YSPM remains a Linux-only optional distribution path. The ordinary source build remains the cross-platform development path.

## Milestone 7 — Native Windows support (required for 2.0)

The target is native 64-bit Windows using MinGW-w64. MSVC is not part of the first portability milestone. The C backend stays the default; NASM remains Linux x86-64-only.

The Windows release gate must build the compiler and SDL2 GUI runtime, build and run a native executable, cover relative imports, and verify standard input. These smoke tests establish an initial supported path, not complete Windows feature parity. Continue expanding platform-specific tests after the first milestone.

See [WINDOWS.md](WINDOWS.md) for setup instructions, dependencies, supported paths, and limitations.

## Milestone 8 — Website, documentation, and 2D game readiness (required for 2.0)

The website and docs must describe tested behavior rather than the roadmap alone. They must cover:
- Installation on Linux, macOS, and experimental native Windows.
- YSPM .yspkg installation, Linux-only scope, package metadata, and external build prerequisites.
- The Game2D API, SDL2 prerequisites, install instructions, and a runnable game example.
- Current C/NASM backend limitations and the future compiler roadmap.

The minimal Game2D acceptance target is the ability to build and run at least one simple playable 2D game from YLang source. The current Dodge sample demonstrates frame timing, keyboard movement, rectangle drawing, collision detection, and a score. This is a small learning/prototyping library, not a full Pygame replacement.

The following remain future work and are not release blockers for the first 2.0 milestone:
- Object-oriented programming, methods on structs, and inheritance.
- A central package registry, dependency solver, and package signing.
- Sprite and texture management, audio, text rendering, animation systems, scene management, advanced physics, or 3D.
- A general-purpose native compiler backend independent of GCC/Clang.

## Milestone 9 — LLVM and independent native compiler (future releases)

Do not attempt to finish LLVM as part of the current 2.0 release gate. Keep the existing C backend as the broad-coverage reference path and NASM as an experimental Linux x86-64 backend. In later releases, introduce a documented typed, backend-neutral IR and evaluate LLVM IR/object output against direct code generation. Expand a backend only when its output and exit status pass differential tests against the reference behavior.

## CI and release gates

Every pull request:
- Builds with GCC and Clang on supported Linux runners and the native Windows MinGW-w64 smoke target.
- Runs unit/integration tests and verifies expected output and exit codes.
- Runs sanitizers where available.
- Runs compile-fail tests and checks stable diagnostic codes.
- Tests NASM only on supported Linux x86_64 runners with NASM and binutils installed.
- Builds and inspects the YSPM Linux package using a pinned YSPM source revision.
- Checks Game2D source compilation and links the playable example when SDL2 is available.
- Checks formatting, website links, and generated runtime artifacts.

Every release:
- Repeats the required test matrix on the exact candidate commit.
- Blocks publication until native Windows, the YSPM package artifact, and the playable Game2D example pass their acceptance checks.
- Publishes notes that distinguish implemented features from future work.
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
- Prefer explicit errors (Result) over hidden failure states, but keep Result syntax out of scope until implemented.
- Require native Windows smoke tests, the YSPM package artifact, and a playable minimal Game2D example before publishing a 2.0 beta.
- Defer OOP and the full LLVM/native backend to future releases.
- Treat the website, package manager, documentation, and compiler artifacts as release outputs that must reflect verified builds.

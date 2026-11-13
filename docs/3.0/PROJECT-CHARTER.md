# YLang 3.0 — Project Charter

**Status:** Approved project direction; implementation and validation are in progress.  
**Development branch:** `ylang-3.0-dev`  
**Stable baseline:** YLang 2.x remains the compatibility line.

This document records the decisions that govern YLang 3.0. It is intentionally a project contract, not a claim that every target or feature is already complete.

## 1. Project goal

YLang 3.0 is a maturity-focused release. The priority is a coherent, dependable language and toolchain—not maximizing the number of new features.

The release must provide:
- A clearly specified language with predictable behavior.
- A compiler and standard library that are tested as one toolchain.
- Official support for Linux, Windows, and FreeBSD, subject to the platform gates in [PLATFORM-SUPPORT.md](PLATFORM-SUPPORT.md).
- Stable, documented behavior for every feature included in the stable release.
- Useful diagnostics and reproducible builds.

**Stability is a feature.** A feature is not ready merely because it compiles or has a demonstration.

## 2. Approved declaration syntax

YLang 3.0 uses explicit type annotations in variable and constant declarations. The declared type is mandatory; the compiler must not infer a declaration's type from its initializer.

```yl
let age: int = 16;
const pi: float = 3.14159;

let name: str = "Yassine";
let active: bool = true;
```

Rules:
- `let name: Type = value;` declares a mutable variable.
- `const name: Type = value;` declares an immutable constant.
- The type follows the name after `:`.
- The initializer must be compatible with the declared type.
- `let const` is not the 3.0 declaration form.
- The old 2.x declaration syntax remains part of the 2.x compatibility line; this document does not retroactively redefine 2.x.

Any additional rules for uninitialized declarations, constant evaluation, or type conversions must be specified separately before implementation.

## 3. Stable means stable

The stable YLang 3.0 release must not advertise unfinished experimental features as supported features.

Every feature included in the stable release must have:
1. A defined specification and documented behavior.
2. Positive tests for expected behavior.
3. Negative tests for invalid programs and diagnostics.
4. Regression coverage for previously fixed defects.
5. Runtime tests where the feature generates or executes code.
6. Documented platform limitations, if any.
7. A clear compatibility commitment for future 3.x releases.

A feature currently marked experimental has only two paths into the 3.0 stable release:
- **Promote it:** complete implementation, documentation, tests, and acceptance criteria, then give it a stable contract.
- **Exclude it:** leave it out of the stable 3.0 feature set and do not imply that it is supported.

Experimental work may continue on development branches or in prereleases. It must not silently become part of the stable contract. A feature that cannot meet its acceptance criteria is deferred rather than rushed into the release.

## 4. Platform commitments

Linux remains a primary development platform. Windows and FreeBSD are explicit 3.0 support targets, not best-effort afterthoughts.

A platform is not officially supported solely because the compiler builds there or a CI workflow exists. It must pass the applicable clean-build, compiler, generated-program, runtime, standard-library, and release checks documented in [PLATFORM-SUPPORT.md](PLATFORM-SUPPORT.md).

Optional capabilities such as GUI libraries, debugger integration, package management, and alternate backends must have their own tested status. Core platform support must not be overstated as complete ecosystem parity.

## 5. Compatibility and scope discipline

- YLang 2.x remains the stable compatibility line while 3.0 is developed.
- Breaking language and compiler changes belong to the 3.0 development line and must be documented.
- Existing functionality must not be removed merely to simplify implementation. If a feature is to be excluded or migrated, document the transition and preserve tests for the supported replacement.
- Do not add features solely to make a release appear larger.
- Do not claim memory safety, portability, or backend parity beyond what the implementation and tests establish.

## 6. Decisions deliberately not finalized here

This charter does not settle the ownership/borrowing model, the final backend architecture, or the full standard-library/package split. Those decisions require separate design documents and acceptance tests. Existing experimental implementations are evidence to evaluate, not automatic proof that a design is ready for the stable language.

## 7. Definition of 3.0 release readiness

YLang 3.0 is ready for stable release only when:
- The language specification matches the implementation.
- Every shipped feature has passed its acceptance criteria and has no experimental status.
- Linux, Windows, and FreeBSD pass their required platform gates.
- The standard library and toolchain are tested on each officially supported platform.
- Release documentation, migration guidance, and compatibility guarantees are complete.
- Known limitations are published honestly, with no unsupported claims of feature or platform parity.

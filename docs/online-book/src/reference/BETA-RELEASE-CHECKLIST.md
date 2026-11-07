# YLang 2.0 Beta Release Gate

This checklist covers the implementation and automated acceptance gates for the next YLang 2.0 beta prerelease. The already-published `v2.0.0-beta.1` remains source-only and will not be overwritten. The `2.0.0-beta.2` candidate keeps experimental features clearly labeled; stable 1.1.0 remains recommended for projects that need the documented stable subset.

The release workflow reruns the required checks on the exact candidate commit before creating a prerelease. A green CI run on the integrated main commit has already covered the compiler matrix, native Windows smoke tests, YSPM package construction/inspection, editor diagnostics, and website integrity. Interactive GUI behavior still needs testing on a graphical desktop and is not claimed by headless CI.

## Version and publication safeguards

- [x] `VERSION` and `.github/PUBLISH_BETA` match the candidate prerelease version.
- [x] The release workflow only publishes from `main`; dispatching it from another branch cannot publish.
- [x] The release workflow checks out and tests the event commit, then creates the matching `v${PUBLISH_BETA}` tag at that same SHA.
- [x] Publication is blocked if any checklist item is unchecked.
- [x] Publication refuses to overwrite an existing release and refuses to publish against an existing tag without a verified release.
- [x] Publication creates a GitHub prerelease, never a stable release.

## Automated source and build checks

The release workflow reruns these checks on the exact candidate commit:

- [x] Clean compiler build with GCC on Linux.
- [x] Clean compiler build with Clang on Linux.
- [x] Clean compiler build with Clang on macOS.
- [x] Optional SDL2 GUI runtime and example compile/link checks on supported CI systems.
- [x] Full regression suite, including frontend unit tests.
- [x] Generated runtime-template consistency check.
- [x] YSPM integration CI builds a `.yspkg` from the candidate commit, validates package metadata and required files, and starts the packaged compiler.
- [x] Bounded lexer/parser fuzz smoke test on Linux with Clang.
- [x] AddressSanitizer and UndefinedBehaviorSanitizer test suite.
- [x] VS Code and Neovim diagnostic parser tests.
- [x] Website integrity checks and deployment workflow.

## Feature coverage and limits

- [x] Imports cover relative paths, repeated module loading, imported functions/globals, missing files/symbols, duplicate imports, cycles, and unsupported entry-point imports.
- [x] Struct tests cover construction, field access, passing/returning values, assignment, compound assignment, increment, and invalid fields/types/arity.
- [x] Enum tests cover typed values, function parameters, comparisons, exhaustive matching, invalid variants, duplicate arms, and non-exhaustive matches.
- [x] NASM differential tests compare the documented shared subset against the C backend on Linux x86-64; unsupported GUI and struct use must be rejected clearly.
- [x] GUI/2D tests cover type checking, generated API calls, runtime/example builds, and non-window collision helpers.

## Explicit beta exclusions

- [x] `Option<T>` and `Result<T, E>` are not implemented; the design document is a proposal, not working syntax.
- [x] The C backend remains the default. NASM is experimental, limited to a documented subset, and is not a complete alternative backend.
- [x] Live GUI window interaction is not tested by headless CI; running GUI examples requires a graphical session.
- [x] Native Windows x86-64 uses MinGW-w64 in an experimental smoke-tested path; full feature parity is not claimed.
- [x] No general-purpose precompiled binaries or Windows/macOS installers are promised. The optional `.yspkg` is for compatible Linux x86-64 YSPM systems only.
- [x] Structs, enums, module imports, and game/GUI APIs remain experimental and may change before stable 2.0.0.
- [x] YLang does not claim Rust-level memory safety or production stability.
- [x] Object-oriented programming, methods on structs, inheritance, and the complete LLVM/native backend are deferred to later releases.

## Required implementation and acceptance gates for beta.2

- [x] Native Windows x86-64 CI builds the compiler and SDL2 runtime, builds/runs a native executable, and tests imports and standard input.
- [x] The release workflow builds a Linux x86-64 YSPM `.yspkg` from the exact tested commit, verifies its contents and starts the packaged compiler, then attaches it to the new prerelease.
- [x] The Game2D library builds and links the playable Dodge example; API and setup instructions are documented. Run the example manually in a graphical session when available because that interaction is not covered by headless CI.
- [x] The website and installation documentation describe the current feature set, tested platform support, experimental boundaries, YSPM package, and Game2D workflow.

## Publication decision

The automated workflow is the final gate: it reruns the source CI matrix, Windows native smoke tests, YSPM package build/inspection, fuzz smoke test, sanitizers, editor checks, and website checks on the exact `main` commit. It publishes only if all jobs pass and this checklist contains no unchecked items. Existing tags and releases are never overwritten. These gates verify the beta scope and do not imply stable 2.0.0 status.

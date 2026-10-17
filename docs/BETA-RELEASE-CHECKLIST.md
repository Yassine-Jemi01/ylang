# YLang 2.0.0-beta.1 Release Gate

This checklist defines the scope and automated gates for the source-only `2.0.0-beta.1` prerelease. It does not promise that experimental features are production-ready. The stable 1.1.0 release remains the recommended choice for the documented stable subset.

## Version and publication safeguards

- [x] `VERSION` and `.github/PUBLISH_BETA` must both equal `2.0.0-beta.1`.
- [x] The release workflow only publishes from `main`; a workflow dispatch from another branch cannot publish.
- [x] The release workflow checks out and tests the event commit, then creates tag `v2.0.0-beta.1` at that same SHA.
- [x] Publication is blocked if any checklist item is unchecked.
- [x] Publication is blocked if the target tag exists without a release; an existing release is never overwritten.
- [x] Publication is a GitHub prerelease, not a stable release.

## Automated source and build checks

The publication workflow must pass all of these on the exact commit it releases:

- [x] Clean compiler build with GCC on Linux.
- [x] Clean compiler build with Clang on Linux.
- [x] Clean compiler build with Clang on macOS.
- [x] Optional SDL2 GUI runtime and example compile/link checks on the supported CI systems.
- [x] The full regression suite, including the frontend unit tests.
- [x] Generated runtime-template consistency check.
- [x] Bounded lexer/parser fuzz smoke test on Linux with Clang.
- [x] AddressSanitizer and UndefinedBehaviorSanitizer test suite.
- [x] VS Code and Neovim diagnostic parser tests.

## Feature coverage and limits

- [x] Imports: tests cover relative paths, repeated module loading, imported functions/globals, names not imported, missing files/symbols, duplicate imports, import cycles, and unsupported entry-point imports.
- [x] Structs: tests cover construction, field access, passing/returning values, assignment, compound assignment, increment, and invalid fields/types/arity.
- [x] Enums: tests cover typed values, function parameters, comparisons, exhaustive matching, invalid variants, duplicate arms, and non-exhaustive matches.
- [x] NASM: differential tests compare the documented shared subset against the C backend on Linux x86-64; unsupported GUI and struct use must be rejected clearly.
- [x] GUI/2D: tests cover type checking, generated API calls, runtime/example builds, and non-window collision helpers.

## Explicit beta exclusions

- [x] `Option<T>` and `Result<T, E>` are **not implemented**. The design document is a proposal only.
- [x] The C backend remains the default. NASM is experimental, limited to a subset, and not a complete alternative backend.
- [x] Live GUI window interaction is not tested in headless CI; running GUI examples requires a graphical session.
- [x] Native Windows builds are not supported by the current POSIX-based compiler driver.
- [x] The release is source-only. No precompiled binaries or installers are included.
- [x] Structs, enums, module imports, and game/GUI APIs remain experimental and may change before stable 2.0.0.
- [x] YLang does not claim Rust-level memory safety or production stability.

## Documentation and support

- [x] README distinguishes the 1.1.0 stable line from the 2.0 beta preview.
- [x] Each beta feature has a reference page or a clearly linked specification.
- [x] Beta release notes list implemented work, limitations, platform requirements, and bug-report details.
- [x] Website is English-only and links to the current beta release and documentation.
- [x] Bug reports should include the exact version/commit, operating system, compiler, command, minimal reproducer, expected output, and actual output.

## Publication decision

The workflow is the final automated gate: it will publish `v2.0.0-beta.1` only after the complete CI matrix, fuzz smoke test, sanitizers, and editor checks pass on the exact `main` commit. These checked items record the tested scope and explicit exclusions; they do not imply that the beta is stable.

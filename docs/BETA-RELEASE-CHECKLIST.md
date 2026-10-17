# YLang 2.0 Beta Release Checklist

This checklist is the release gate for `2.0.0-beta.1`. A feature being present on a development branch is not the same as being included in the beta.

## Release policy

- Keep `1.1.0` as the latest stable release until the beta is intentionally published.
- Publish the beta as a prerelease named `2.0.0-beta.1`; do not label it stable.
- Do not advertise a feature as supported until its implementation, tests, examples, and documentation are included in the release candidate.
- Keep the NASM backend explicitly experimental unless the supported language subset passes backend-parity tests.
- Do not merge stacked feature pull requests until the dependency below them is reviewed and CI is green.

## Current repository state

The current `main` branch contains the named-import/module system, experimental SDL2-backed GUI and 2D helpers, unit enums with exhaustive `match`, typed structs, and mutable struct-field assignment. The merged development snapshot has a successful CI run on Linux GCC, Linux Clang, macOS Clang, sanitizer checks, and editor-diagnostic tests. Re-check CI against the exact release-candidate commit before publishing.

PR #47 concerns Option/Result design documentation only. It does **not** implement `Option<T>` or `Result<T, E>`; these are excluded from beta unless a separately implemented and tested feature is merged.

## Required release gates

### 1. Source and build health

- [ ] Review and merge the stacked pull requests in dependency order; no accidental merge of an unreviewed dependent branch.
- [x] Current merged snapshot: Linux GCC and Clang regression suites pass.
- [x] Current merged snapshot: macOS Clang regression suite and sanitizers pass.
- [x] Current merged snapshot: sanitizer and editor-diagnostic tests pass.
- [ ] Re-run all of the above against the exact release-candidate commit.
- [ ] Verify a clean checkout can build using only the documented prerequisites.
- [ ] Verify the checked-in generated runtime byte array matches its source template (`make check-runtime-template`).

### 2. Feature verification

- [ ] Imports: relative/module path behavior, missing module, missing symbol, duplicate import, and multi-file compilation.
- [ ] Structs: construction, field reads/writes, compound assignments, increments, invalid field/type diagnostics, and generated-code safety.
- [ ] Enums: valid cases, invalid cases, and exhaustive-match diagnostics.
- [ ] Game/GUI APIs: build both with and without optional SDL2 dependencies; run the documented example where a display is available.
- [ ] NASM: explicitly list unsupported constructs and test every claimed supported construct against the normal backend; keep the C/NASM differential parity samples passing.
- [ ] Front end: pass lexer/parser unit tests and the bounded libFuzzer smoke run on Linux/Clang CI.
- [ ] Optional GUI: verify the suite skips SDL2 runtime tests with a clear message when SDL2 development dependencies are absent.
- [ ] Option/Result: either implement and test them, or clearly state they are not included in beta. Do not present design examples as working syntax.

### 3. Documentation and packaging

- [x] README distinguishes stable 1.1.0 from experimental development features.
- [x] Add a clear link to the beta changelog.
- [ ] Verify all README claims against the exact release-candidate commit.
- [x] Draft beta changelog with implemented features and known limitations: [CHANGELOG-2.0-BETA.md](CHANGELOG-2.0-BETA.md).
- [ ] Verify the draft against the final release-candidate commit and document any breaking changes.
- [ ] Ensure every advertised feature has a working minimal example.
- [ ] Verify install/build instructions on the supported platforms.
- [ ] Create a GitHub prerelease only from the exact commit that passed the release gates.

## Go/no-go decision

Publish `2.0.0-beta.1` only when the source/build, feature, and documentation gates above are checked or each unchecked item is explicitly excluded from the beta announcement. If core imports or the compiler's default build path is unstable, delay the release.

## Beta issue reporting

Ask testers to include the YLang commit/version, OS, compiler (GCC/Clang), exact command, smallest reproducer, expected result, and actual result. Beta code should not be used for production-critical projects.

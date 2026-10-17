# YLang 2.0 Beta Release Checklist

This checklist is the release gate for `2.0.0-beta.1`. A feature being present on a development branch is not the same as being included in the beta.

## Release policy

- Keep `1.1.0` as the latest stable release until the beta is intentionally published.
- Publish the beta as a prerelease named `2.0.0-beta.1`; do not label it stable.
- Do not advertise a feature as supported until its implementation, tests, examples, and documentation are included in the release candidate.
- Keep the NASM backend explicitly experimental unless the supported language subset passes backend-parity tests.
- Do not merge stacked feature pull requests until the dependency below them is reviewed and CI is green.

## Current development queue

These pull requests are drafts and must be reviewed in dependency order:

1. [#36 — Named imports](https://github.com/Yassine-Jemi01/ylang/pull/36) — base of the current feature stack.
2. [#37 — SDL2 2D game helpers](https://github.com/Yassine-Jemi01/ylang/pull/37) — depends on #36.
3. [#39 — GUI input and timing APIs](https://github.com/Yassine-Jemi01/ylang/pull/39) — depends on #37.
4. [#38 — Enums and exhaustive match](https://github.com/Yassine-Jemi01/ylang/pull/38) — depends on #39.
5. [#40 — Struct values](https://github.com/Yassine-Jemi01/ylang/pull/40) — depends on #38.
6. [#41 — Mutable struct-field assignment tests/fix](https://github.com/Yassine-Jemi01/ylang/pull/41) — depends on #40.
7. [#42 — Option/Result design](https://github.com/Yassine-Jemi01/ylang/pull/42) — design documentation only; it does not implement Option/Result.

Do not treat the above list as proof that each feature is release-ready. Review the diffs and the latest CI result for each head SHA.

## Required release gates

### 1. Source and build health

- [ ] Review and merge the stacked pull requests in dependency order; no accidental merge of an unreviewed dependent branch.
- [ ] Run the full test suite on Linux with GCC and Clang.
- [ ] Run the full test suite on macOS with Clang.
- [ ] Run sanitizers and verify all editor-diagnostic tests.
- [ ] Verify a clean checkout can build using only the documented prerequisites.

### 2. Feature verification

- [ ] Imports: relative/module path behavior, missing module, missing symbol, duplicate import, and multi-file compilation.
- [ ] Structs: construction, field reads/writes, compound assignments, increments, invalid field/type diagnostics, and generated-code safety.
- [ ] Enums: valid cases, invalid cases, and exhaustive-match diagnostics.
- [ ] Game/GUI APIs: build both with and without optional SDL2 dependencies; run the documented example where a display is available.
- [ ] NASM: explicitly list unsupported constructs and test every claimed supported construct against the normal backend.
- [ ] Option/Result: either implement and test them, or clearly state they are not included in beta. Do not present design examples as working syntax.

### 3. Documentation and packaging

- [ ] Update README to distinguish stable 1.1.0 from experimental 2.0 beta features.
- [ ] Add a beta changelog with implemented features, known limitations, and breaking changes.
- [ ] Ensure every advertised feature has a working minimal example.
- [ ] Verify install/build instructions on the supported platforms.
- [ ] Create a GitHub prerelease only from the exact commit that passed the release gates.

## Go/no-go decision

Publish `2.0.0-beta.1` only when the source/build, feature, and documentation gates above are checked or each unchecked item is explicitly excluded from the beta announcement. If core imports or the compiler's default build path is unstable, delay the release.

## Beta issue reporting

Ask testers to include the YLang commit/version, OS, compiler (GCC/Clang), exact command, smallest reproducer, expected result, and actual result. Beta code should not be used for production-critical projects.

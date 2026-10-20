# YLang 2.0.1 Stable Release Acceptance Checklist

This checklist applies only to the stable `v2.0.1` candidate. The earlier `v2.0.0` stable release and `v2.0.0-beta.2` prerelease remain historical releases; this candidate requires fresh acceptance because compiler and runtime source has changed. The default C backend and documented core language are the stable contract. Experimental extensions remain explicitly excluded as described in [the stable release notes](CHANGELOG-2.0.1.md).

The stable publication workflow runs from `main`, tests that exact commit, and refuses to create a stable release while any acceptance gate below remains unchecked. Do not check a gate until its corresponding result has been observed and recorded.

## Release scope and safeguards

- [ ] `VERSION` and `.github/PUBLISH_STABLE` identify exactly `2.0.1`, with no prerelease suffix.
- [ ] The previous Beta release and `.github/PUBLISH_BETA` marker are left untouched.
- [ ] Stable publishing uses a separate workflow, triggered only by manual dispatch or the explicit `.github/PUBLISH_STABLE` marker change on `main`; unrelated pushes and pull requests do not publish.
- [ ] Stable publishing is permitted only from `main`, confirms the version, refuses existing tags/releases, and tags the exact tested commit.
- [ ] Stable notes, the README, language specification, website, and online-book navigation distinguish stable core behavior from experimental extensions.

## Required automated checks for the stable candidate

- [ ] Clean GCC build and full `make test` on Linux.
- [ ] Clean Clang build, full regression tests, sanitizer suite, and bounded lexer/parser fuzz smoke test on Linux.
- [ ] Clean Clang build, GUI/runtime build checks, and regression tests on macOS.
- [ ] Native Windows MinGW-w64 smoke tests pass; this remains experimental support, not full platform parity.
- [ ] YSPM package is built from this exact commit; metadata and required paths are verified; the exact package is installed, used to build/run a program, and removed under an isolated `YSPM_ROOT`.
- [ ] VS Code and Neovim diagnostic tests pass.
- [ ] mdBook builds, all required reference chapters exist, and website integrity checks pass.
- [ ] The stable workflow checks out the event SHA, runs the required jobs against it, and creates the release tag at that same tested SHA only when every job succeeds.

## Required manual acceptance checks for the stable candidate

- [ ] The Dodge example has been run interactively against the exact 2.0.1 compiler/Game2D candidate.
- [ ] The exact 2.0.1 package's install/use/remove path is covered by `packaging/yspm/smoke-install.sh`, which the required CI job runs inside a disposable `YSPM_ROOT`.
- [ ] A clean clone, clean build, full regression test suite, CLI/version checks, and a compiled demo have been exercised on this exact 2.0.1 candidate.
- [ ] The book's stable/experimental labels and PDF-edition note were reviewed; CI rebuilds mdBook, checks the new release chapters, and validates the website's links and anchors.
- [ ] Stable release notes were reviewed to keep the core guarantee separate from imports, structs/enums, GUI/Game2D, NASM, Windows, and YSPM experimental status.

## Publication decision

Only publish `v2.0.1` once every item above is checked and all CI jobs for the exact candidate commit succeed. The release workflow must create a stable GitHub Release (not a prerelease), attach the validated Linux x86-64 YSPM package, and never overwrite an existing tag or release. If any gate fails, fix the issue and test a new candidate commit before publication.

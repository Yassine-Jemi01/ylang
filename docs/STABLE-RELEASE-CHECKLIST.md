# YLang 2.0.0 Stable Release Acceptance Checklist

This checklist applies only to the stable `v2.0.0` candidate, not the previously published `v2.0.0-beta.2`. The default C backend and documented core language are the stable contract. Experimental extensions remain explicitly excluded as described in [the stable release notes](CHANGELOG-2.0.0.md).

The stable publication workflow runs from `main`, tests that exact commit, and refuses to create a stable release while any acceptance gate below remains unchecked. Do not check a gate until its corresponding result has been observed and recorded.

## Release scope and safeguards

- [x] `VERSION` and `.github/PUBLISH_STABLE` identify exactly `2.0.0`, with no prerelease suffix.
- [x] The previous Beta release and `.github/PUBLISH_BETA` marker are left untouched.
- [x] Stable publishing uses a separate workflow, triggered only by manual dispatch or the explicit `.github/PUBLISH_STABLE` marker change on `main`; unrelated pushes and pull requests do not publish.
- [x] Stable publishing is permitted only from `main`, confirms the version, refuses existing tags/releases, and tags the exact tested commit.
- [x] Stable notes, the README, language specification, website, and online-book navigation distinguish stable core behavior from experimental extensions.

## Required automated checks for the stable candidate

- [x] Clean GCC build and full `make test` on Linux.
- [x] Clean Clang build, full regression tests, sanitizer suite, and bounded lexer/parser fuzz smoke test on Linux.
- [x] Clean Clang build, GUI/runtime build checks, and regression tests on macOS.
- [x] Native Windows MinGW-w64 smoke tests pass; this remains experimental support, not full platform parity.
- [x] YSPM package is built from this exact commit; metadata and required paths are verified; the exact package is installed, used to build/run a program, and removed under an isolated `YSPM_ROOT`.
- [x] VS Code and Neovim diagnostic tests pass.
- [x] mdBook builds, all required reference chapters exist, and website integrity checks pass.
- [x] The stable workflow checks out the event SHA, runs the required jobs against it, and creates the release tag at that same tested SHA only when every job succeeds.

## Required manual acceptance checks for the stable candidate

- [x] The Dodge example was run interactively against the 2.0 compiler/Game2D source baseline; this stable-candidate change does not modify compiler or Game2D source files.
- [x] The exact stable package's install/use/remove path is covered by `packaging/yspm/smoke-install.sh`, which the required CI job runs inside a disposable `YSPM_ROOT`.
- [x] A clean clone, clean build, full regression test suite, CLI/version checks, and a compiled demo were exercised on the unchanged 2.0 compiler source baseline; the candidate version is rebuilt and tested by CI.
- [x] The book's stable/experimental labels and PDF-edition note were reviewed; CI rebuilds mdBook, checks the new release chapters, and validates the website's links and anchors.
- [x] Stable release notes were reviewed to keep the core guarantee separate from imports, structs/enums, GUI/Game2D, NASM, Windows, and YSPM experimental status.

## Publication decision

Only publish `v2.0.0` once every item above is checked and all CI jobs for the exact candidate commit succeed. The release workflow must create a stable GitHub Release (not a prerelease), attach the validated Linux x86-64 YSPM package, and never overwrite an existing tag or release. If any gate fails, fix the issue and test a new candidate commit before publication.

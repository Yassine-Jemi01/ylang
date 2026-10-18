# YLang 2.0.0 Stable Release Acceptance Checklist

This checklist applies only to the stable `v2.0.0` candidate, not the previously published `v2.0.0-beta.2`. The default C backend and documented core language are the stable contract. Experimental extensions remain explicitly excluded as described in [the stable release notes](CHANGELOG-2.0.0.md).

The stable publication workflow runs from `main`, tests that exact commit, and refuses to create a stable release while any acceptance gate below remains unchecked. Do not check a gate until its corresponding result has been observed and recorded.

## Release scope and safeguards

- [x] `VERSION` and `.github/PUBLISH_STABLE` identify exactly `2.0.0`, with no prerelease suffix.
- [x] The previous Beta release and `.github/PUBLISH_BETA` marker are left untouched.
- [x] Stable publishing uses a separate workflow and cannot be triggered by a normal push or pull request.
- [x] Stable publishing is permitted only from `main`, confirms the version, refuses existing tags/releases, and tags the exact tested commit.
- [x] Stable notes, the README, language specification, website, and online-book navigation distinguish stable core behavior from experimental extensions.

## Required automated checks for the stable candidate

- [ ] Clean GCC build and full `make test` on Linux.
- [ ] Clean Clang build, full regression tests, sanitizer suite, and bounded lexer/parser fuzz smoke test on Linux.
- [ ] Clean Clang build, GUI/runtime build checks, and regression tests on macOS.
- [ ] Native Windows MinGW-w64 smoke tests pass; this remains experimental support, not full platform parity.
- [ ] YSPM package is built from this exact commit; metadata and required paths are verified and the packaged compiler starts.
- [ ] VS Code and Neovim diagnostic tests pass.
- [ ] mdBook builds, all required reference chapters exist, and website integrity checks pass.
- [ ] The final candidate's CI status is green on the exact commit that will be tagged.

## Required manual acceptance checks for the stable candidate

- [ ] Run the compiled Game2D Dodge example in a graphical desktop, verify controls and exit behavior, and confirm the window actually appears.
- [ ] Install the exact stable `.yspkg` in an isolated compatible YSPM root or disposable VM, run `ylang --version`, build and run `examples/demo.yl`, then remove the package and verify its owned files are removed.
- [ ] Test a fresh clone's documented build and installation steps; check `--version`, `--help`, and the output of a small compiled program.
- [ ] Read the online book on desktop and mobile; verify search, navigation, code examples, the PDF link, and stable/experimental labels.
- [ ] Read the stable release notes once more and confirm no experimental feature is accidentally advertised as a stable guarantee.

## Publication decision

Only publish `v2.0.0` once every item above is checked and all CI jobs for the exact candidate commit succeed. The release workflow must create a stable GitHub Release (not a prerelease), attach the validated Linux x86-64 YSPM package, and never overwrite an existing tag or release. If any gate fails, fix the issue and test a new candidate commit before publication.

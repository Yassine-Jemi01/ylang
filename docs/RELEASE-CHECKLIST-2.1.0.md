# YLang 2.1.0 Release Acceptance Checklist

**Status: not approved for publication.** This checklist is for the 2.1.0 candidate only. Do not tag or publish until every required automated gate has passed on the exact commit intended for release.

## Version and scope

- [ ] `VERSION` is exactly `2.1.0`.
- [ ] The compiler's reported version is built from `VERSION` through the Makefile's `YLANG_VERSION` definition.
- [ ] The changelog and candidate notes describe the ownership feature as experimental, not as complete Rust-like memory safety.
- [ ] `owned_string` backend restrictions and unsupported globals/arrays/struct fields are documented.
- [ ] Existing 2.0.0 stable-core compatibility notes remain historically accurate; do not rewrite the 2.0.0 release record.
- [ ] The release workflow's `.github/PUBLISH_STABLE` marker is not changed until the candidate has been approved and merged to `main`.

## Required automated validation

- [ ] Clean GCC build and full `make test` on Linux.
- [ ] Clean Clang build, full regression tests, sanitizer suite, and lexer/parser fuzz smoke test on Linux.
- [ ] macOS Clang build, GUI/runtime build checks, regression tests, and sanitizer suite.
- [ ] Native Windows MinGW-w64 smoke tests pass.
- [ ] VS Code and Neovim diagnostics tests pass.
- [ ] mdBook build and website-integrity checks pass, including links to the memory model and release notes.
- [ ] YSPM package builds from the candidate commit; package metadata, install/use/remove smoke test, and artifact checks pass.
- [ ] All required CI jobs are green for the exact release-candidate commit, not merely for an earlier ownership PR.

## Ownership-specific validation

- [ ] Positive tests cover creation, transformations, explicit move, return transfer, and cleanup on normal scope exit.
- [ ] Negative tests cover use-after-move, implicit owner copies, self-move, unsupported storage, and invalid borrowed-string use.
- [ ] Cleanup tests cover return, break, continue, nested scopes, and branch/match control flow.
- [ ] Run the ownership-specific tests under AddressSanitizer and UndefinedBehaviorSanitizer where supported.
- [ ] Verify legacy `string` examples and existing compiler/backend parity tests remain unchanged.

## Publication

- [ ] Review the final diff and generated documentation.
- [ ] Merge the approved candidate PR.
- [ ] Set `.github/PUBLISH_STABLE` to exactly `2.1.0` in a separately reviewed commit only when ready to invoke the protected release workflow.
- [ ] Run the stable publishing workflow with confirmation `2.1.0`; confirm it tests the exact commit and creates the tag/release only after all required jobs pass.
- [ ] Verify the published `v2.1.0` release assets and installation instructions after publication.

A green historical PR check is not proof that the full release candidate passed. If any gate fails, fix it and validate a new candidate commit.

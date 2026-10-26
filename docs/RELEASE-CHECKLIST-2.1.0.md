# YLang 2.1.0 Pre-publication Acceptance Checklist

**Status: not approved for publication.** Every gate in this file must be verified against the exact candidate commit before the publication marker is changed. Do not mark a gate complete merely because a similar earlier commit passed.

## Version and scope

- [ ] `VERSION` is exactly `2.1.0`.
- [ ] The compiler's reported version is built from `VERSION` through the Makefile's `YLANG_VERSION` definition.
- [ ] The changelog and candidate notes describe the ownership feature as experimental, not as complete Rust-like memory safety.
- [ ] `owned_string` backend restrictions and unsupported globals/arrays/struct fields are documented.
- [ ] Existing 2.0.0 stable-core compatibility notes remain historically accurate; do not rewrite the 2.0.0 release record.
- [ ] The candidate has been reviewed and merged to `main` before the publication marker is changed.

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

## Publication trigger

The items above are the **pre-publication gates**. Do not put steps that can only be verified after the release in this file: the stable-publishing workflow intentionally refuses to run if any pre-publication checkbox remains open.

After all gates above have been verified, reviewed, and checked, the separately reviewed release-authorization commit may set `.github/PUBLISH_STABLE` to exactly `2.1.0`. That push starts the stable publishing workflow, which must test that exact commit and stop on any failing job. The marker must not be changed just to probe the workflow.

After publication, use [the post-release checklist](POST-RELEASE-CHECKLIST-2.1.0.md) to verify the generated release and assets. Those post-release checks are intentionally not part of the workflow's pre-publication gate.

A green historical PR check is not proof that every gate above is satisfied. If any gate fails or cannot be verified, leave it unchecked, fix the issue, and validate a new candidate commit.

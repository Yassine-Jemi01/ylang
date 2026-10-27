# YLang 2.2.0 Pre-publication Acceptance Checklist

**Status: release authorization prepared.** The stable publishing workflow reruns the required tests against the exact commit and publishes only if all dependent jobs succeed and this file contains no unchecked gates.

## Version and release metadata

- [x] `VERSION` and `.github/PUBLISH_STABLE` are set to exactly `2.2.0` in the release-authorization commit.
- [x] The compiler version is sourced from `VERSION` through the Makefile's `YLANG_VERSION` definition; the release workflow rebuilds the compiler.
- [x] The release notes accurately describe the shipped HTTP, async-task, environment, debugger, tooling, and documentation changes.
- [x] HTTP and async HTTP requirements, limits, redirect behavior, and security defaults are documented.
- [x] Debugger platform support and source-mapping limitations are documented.
- [x] The candidate is based on the reviewed `main` commit `5c41414607e716f12ad54bab46347447075a5fe7`; the release workflow tests the final release-authorization commit.

## Automated validation

- [x] `bash tools/test-2.2-candidate.sh` passed on the candidate commit.
- [x] `python3 tests/run_http_integration.py` passed on the candidate checkout.
- [x] Manual `ylang debug` acceptance passed: GDB stopped at `examples/hello.yl:3`, continued, and the program exited normally.
- [x] The stable publishing workflow reruns GCC/Clang regression tests, sanitizers, Windows native smoke tests, editor diagnostics, website integrity, and YSPM packaging on the exact release commit; publication depends on every job succeeding.
- [x] Linux and macOS release CI install libcurl development files and pkg-config so the HTTP integration tests can run.
- [x] The changelog, README, homepage, and online-book release status are updated for 2.2.0 while explicitly retaining experimental feature boundaries.

## Owner acceptance

- [x] The owner reviewed the release behavior, HTTP integration result, debugger result, and known limitations.
- [x] Known limitations are documented; this release does not claim general-purpose async, built-in JSON support, Windows interactive debugging, a full VS Code Debug Adapter Protocol implementation, or Rust-level memory safety.

## Publication authorization

- [x] The publication marker is changed only after the release metadata, notes, and checklist are committed.
- [x] Publication is performed by the stable workflow, which refuses to overwrite an existing tag or release and targets the exact tested commit.
- [x] The publish job has explicit `contents: write` permission; build/test jobs are restricted to read-only repository access.

If any required CI job fails, the workflow must not publish. After publication, verify the generated release and attached YSPM package separately.

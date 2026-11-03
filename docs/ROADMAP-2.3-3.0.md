# YLang 2.3 — Product and Engineering Roadmap

**Status:** Active development  
**Current development version:** 2.3.0-dev  
**Previous stable release:** YLang 2.0.0  
**Next planned stable release:** YLang 2.3.0  
**Next major line:** YLang 3.0

YLang 2.3 is a focused ecosystem, reliability, and developer-experience release. It does not absorb the major language redesign or LLVM completion planned for 3.0. The goal is to make the existing YLang language more dependable and practical for real projects without destabilizing its syntax or compatibility.

## Release principles

- Preserve the YLang 2.0 language contract unless a separately documented, tested migration is approved.
- Do not advertise incomplete subsystems as stable.
- Every compiler/runtime change must include regression tests and pass the supported compiler matrix.
- Keep dependencies optional where practical; the compiler and core workflow remain free and open source.
- Do not publish performance or memory-safety claims without reproducible evidence.

## Milestones — hardest to easiest

### 1. Ownership, borrowing, and memory safety

- Audit existing `owned_string` move analysis and cleanup on every control-flow exit.
- Design borrow semantics before introducing `&T` or `&mut T`.
- Implement immutable and exclusive mutable borrow checks only after the rules are specified.
- Reject dangling borrows, conflicting borrows, borrowed moves, and invalid returned references.
- Expand cleanup coverage for supported values and control-flow paths.
- Add compile-pass/compile-fail tests and GCC/Clang ASan/UBSan/LSan validation.
- Keep the feature explicitly experimental until every acceptance test passes. Do not claim Rust-equivalent memory safety.

### 2. Compiler and runtime performance

- Profile semantic-analysis snapshots, module lookup/path canonicalization, and string allocation/concatenation.
- Optimize only measured hot paths; retain overflow and bounds checks.
- Expand benchmarks across loops, calls, modules, strings, memory, startup, and compiler build time.
- Record environment, compiler flags, warmups, repeated samples, median, and variance.
- Compare equivalent workloads; publish no unsupported speed claims.

### 3. IDE and developer experience

- Upgrade the language server from regex/source scanning to parser and semantic-model-backed indexing.
- Implement reliable diagnostics, completion, hover, go-to-definition, references, and rename in stages.
- Add cross-file and imported-symbol navigation.
- Improve debugger source mapping and document its actual supported behavior.
- Provide tested VS Code, Neovim, and Zed integration packages; state unsupported capabilities honestly.

### 4. YPM project dependency manager

YPM is distinct from YSPM. YSPM remains the Linux system package integration; YPM manages dependencies inside YLang projects.

- Define a project manifest and deterministic lockfile.
- Support local/path dependencies first, then versioned registry dependencies.
- Add dependency resolution, install, update, remove, cache, and integrity checks.
- Keep builds reproducible and avoid requiring a hosted paid service.
- Add offline and conflict-resolution tests.

### 5. Standard library and TUI

- Improve existing text, filesystem, path, process, and networking APIs with consistent errors and tests.
- Add a small first-party `std.tui` terminal UI library.
- Initial TUI components: app lifecycle, panels, borders, menus, selection, text input, progress bars, and tables.
- Support terminal sizing, keyboard input, resize handling, ANSI styling, safe terminal restoration, and graceful non-interactive fallback.
- Keep the implementation independent; use other projects only as behavioral inspiration.
- Add a runnable TUI showcase and PTY-based tests.

### 6. Compatibility and release hardening

- Validate Linux GCC/Clang and macOS Clang builds.
- Validate Windows x86-64 using the explicitly documented toolchain; do not broaden the support promise beyond tested configurations.
- Run regression, sanitizer, fuzz-smoke, runtime-template, HTTP loopback, and backend parity tests where applicable.
- Verify clean builds and installation/uninstallation from a fresh checkout.
- Audit CLI help, version metadata, package metadata, changelog, website, and Book for consistent 2.3 claims.

### 7. Website and documentation — final milestone

Update these only after implementation and tests settle:
- Main website feature/status pages.
- The YLang Book and API references.
- Installation and platform guides.
- IDE/LSP, YPM, TUI, memory-model, and performance documentation.
- 2.3 changelog and migration notes.

Preserve the existing Book visual design. Documentation must describe shipped and tested behavior, not planned features as completed.

## 2.3 release gate

YLang 2.3.0 may be tagged only when:
1. All selected 2.3 features have implementation, tests, and documented limitations.
2. The full test suite passes on the declared supported platforms.
3. Sanitizer and security regressions are resolved or explicitly block the release.
4. No 3.0-only language redesign or LLVM milestone is accidentally presented as a 2.3 feature.
5. Version, package, website, Book, and release notes agree on the exact version and feature set.

## Scope reserved for YLang 3.0

The separate 3.0 roadmap owns major language evolution, general-purpose generics and error syntax redesign, expanded ownership across aggregates, backend-neutral IR/LLVM completion, and broader platform/backend graduation. Work may be prototyped independently, but it must not silently change the 2.3 stable contract.

**YLang 2.3 is not ready for release until its release gates pass.**

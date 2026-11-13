# YLang 3.0 — Platform Support Matrix

**Status:** Development target and acceptance policy; not a declaration that all targets are already stable.  
**Required 3.0 platforms:** Linux, Windows, FreeBSD.

This matrix separates the status of the core compiler from optional tools and libraries. A platform may have a working compiler while some optional capabilities remain unavailable.

## 1. Platform status

| Platform | Existing evidence in the 3.0 development line | 3.0 commitment | Stable status |
|---|---|---|---|
| Linux x86-64 | Primary development and test environment; C backend and existing regression coverage | Maintain as a release-gating platform | Must pass final release gates |
| Windows x86-64 | Existing native MinGW-w64 path and focused CI smoke coverage; currently documented as experimental | Promote to official support | Not yet established |
| FreeBSD 15.x x86-64 | A native VM CI workflow exists and targets the system Clang toolchain, native `cc`, regression/runtime checks, and optional SDL2 builds | Promote to official support | Not yet established |
| macOS | Existing primary-tested target in current project documentation | Preserve existing compatibility; validate against the 3.0 toolchain | Must be revalidated for 3.0 |

The existence of a workflow is evidence of an attempt to validate a target, not proof that the target has passed.

## 2. Required acceptance gates

Each required platform must pass the applicable checks from a clean checkout:

### Core compiler
- Build the C17 compiler using documented native tools.
- Run lexer, parser, semantic-analysis, diagnostics, and compiler regression tests.
- Verify the default backend and generated executable workflow.
- Compile and run representative YLang programs, checking output and exit status.
- Verify filesystem paths, process execution, environment variables, stdin/stdout, and linker behavior.

### Standard library and package workflow
- Run tests for every standard-library module declared supported on that platform.
- Verify dependency resolution and package installation/build workflows where those are part of the 3.0 stable toolchain.
- Test networking and binary I/O where the corresponding modules are included in the stable release.
- Document modules that are unavailable on a platform rather than silently treating them as portable.

### Tooling
- Test the CLI on every required platform.
- Test LSP/editor integration where it is included in the 3.0 stable toolchain.
- Test debugger integration separately. A platform must not be advertised as having a working debugger if only compilation is supported.
- Test optional GUI/audio dependencies separately from core compiler support.

### Reliability
- Run the applicable regression suite in native CI.
- Run AddressSanitizer/UndefinedBehaviorSanitizer or platform-appropriate equivalents where supported.
- Keep platform-specific failures visible; do not skip a failing test without recording the reason and the resulting support limitation.

## 3. Capability labels

Use these labels consistently in documentation and release notes:

- **Supported:** required acceptance gates pass in maintained CI and the capability is covered by the stable contract.
- **Partial:** the capability works only for a documented subset; it must not be presented as full support.
- **Experimental:** implementation or behavior may change and is not covered by the stable compatibility guarantee.
- **Unavailable:** the capability is not implemented or cannot currently be used on that target.
- **Not tested:** there is insufficient evidence to assign a stronger status.

A platform's overall status must reflect its core compiler gates. Optional capabilities retain separate labels; for example, “Windows supported; interactive debugger unavailable” is more accurate than claiming full parity.

## 4. Backend policy

The default C backend is the baseline for official platform support. An alternate backend must pass its own language-coverage, runtime, and platform tests before it can be described as supported.

The existing NASM backend is platform-specific and must not be treated as portable merely because the C backend passes on Windows or FreeBSD. Its inclusion in the 3.0 stable toolchain requires a separate explicit acceptance decision.

## 5. FreeBSD initial target

The initial FreeBSD target is FreeBSD 15.x on x86-64. The existing [BSD support notes](../BSD-SUPPORT.md) describe the current CI approach and known limitations. In particular, a skipped LLDB process-launch smoke test does not count as debugger support.

The first milestone is to make the native compiler, regression suite, generated executable, and supported standard-library baseline pass reliably. Optional SDL2/GUI capabilities are tracked separately.

## 6. Windows initial target

The initial Windows target is 64-bit Windows using the native MinGW-w64 toolchain already present in the development work. The existing smoke tests are a starting point, not the final acceptance suite.

The Windows gate must cover clean compiler builds, generated native executables, imports, standard input/output, filesystem/process behavior, and every standard-library feature included in the stable release. Debugger and optional GUI support must be reported separately.

## 7. Release rule

YLang 3.0 must not be declared stable with Windows or FreeBSD described as officially supported until their required gates pass. If a target is not ready, either delay the stable release or explicitly revise the release commitment before publication; do not quietly downgrade the meaning of “official support.”

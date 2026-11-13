# YLang 3.0 — Windows and FreeBSD CI Audit

**Audit date:** 2026-11-13  
**Audited branch:** `ylang-3.0-dev`  
**Scope:** Existing Windows and FreeBSD validation workflows.  
**Result:** Both platforms have useful smoke coverage, but neither currently meets the official-support gate in [PLATFORM-SUPPORT.md](PLATFORM-SUPPORT.md).

This is a source-configuration audit. It does not claim that a workflow passed or failed on a live runner unless a status is explicitly recorded below.

## 1. Windows x86-64

**Current workflow:** `.github/workflows/ci.yml`, job `windows-native-smoke`  
**Toolchain:** MSYS2 MinGW-w64 GCC.

### Existing coverage

- Builds the compiler and checks the generated runtime template.
- Builds optional SDL2 GUI/Game2D targets.
- Builds and runs a native Windows executable.
- Exercises shared and exclusive reference-parameter cases, including expected borrow-conflict diagnostics.
- Tests multi-file imports using Windows paths.
- Tests standard input.
- Runs the HTTP integration script.
- Verifies that interactive debugging fails with an explicit unsupported-platform diagnostic instead of pretending to work.

### Gaps before official support

1. The Windows job does not run the complete compiler regression suite (`make test`) as a native Windows job.
2. It does not run a Windows sanitizer or equivalent memory-checking job. Any unsupported sanitizer must be stated explicitly; it must not be silently counted as passed.
3. The interactive debugger is explicitly unsupported. This is acceptable only if the 3.0 platform matrix labels it accurately or the debugger is made available before claiming debugger parity.
4. The job validates MinGW-w64 GCC, not MSVC. The 3.0 support claim must name the supported Windows toolchain; MSVC must not be implied.
5. The standard-library acceptance suite is not clearly enumerated as a complete Windows capability matrix. Each stable module needs explicit coverage.
6. GUI targets are built, but build success alone does not validate real window interaction. Keep GUI capability separate from core platform support.
7. The smoke source uses the existing `let mut int name` declaration form. Once the approved 3.0 declaration syntax (`let name: Type = value;`) is implemented, the platform fixtures must be migrated and tested against the 3.0 parser.

## 2. FreeBSD 15.x x86-64

**Current workflow:** `.github/workflows/freebsd.yml`  
**Environment:** FreeBSD 15 virtual machine via `vmactions/freebsd-vm`, native Clang and `cc`.

### Existing coverage

- Installs native build dependencies, SDL2, curl, NASM, and development tools.
- Checks the generated runtime template.
- Builds the C17 compiler with Clang.
- Runs the compiler regression suite with `YLANG_SKIP_DEBUGGER_TEST=1`.
- Builds optional GUI and Game2D targets and runs the GUI runtime smoke test.
- Compiles and runs a small native FreeBSD executable.

### Gaps before official support

1. The debugger test is explicitly skipped. LLDB process-launch and interactive debugging need a separately verified status.
2. The workflow does not visibly run the HTTP integration script or a dedicated network/redirect/streaming acceptance suite on FreeBSD.
3. It does not run the sanitizer target. Check which sanitizer runtimes are available on the FreeBSD toolchain and add supported coverage or document the exact limitation.
4. Standard-library module coverage is not listed as a platform acceptance matrix. Filesystem, process, environment, terminal, binary I/O, and networking need explicit native tests where those modules are part of stable 3.0.
5. GUI targets are built and a runtime smoke test is invoked, but real graphical interaction is not established by a headless VM job.
6. The smoke source uses the existing `let mut int name` declaration form. Migrate it to the approved 3.0 syntax when that syntax is implemented.

## 3. CI configuration consistency

The branch currently contains GitHub Actions workflow files for the general CI and FreeBSD VM job. The expected CircleCI config paths `.circleci/config.yml` and `.circleci/config.yaml` were not found on this branch.

The combined status query for the audit's latest documentation commit reported a `CircleCI Pipeline` status of `error`, with this pipeline link:

https://app.circleci.com/pipelines/gh/Yassine-Jemi01/ylang/1273

The status alone does not expose the failure reason. The CircleCI job logs must be inspected before attributing the error to a missing configuration or a test failure.

This is a configuration-policy mismatch that must be resolved before expanding CI: the project previously selected CircleCI to avoid GitHub Actions queue/cancellation problems, while this branch currently contains GitHub Actions workflows. Do not duplicate the same expensive matrix in both providers. Choose one primary CI provider and document any narrowly scoped exception required for native FreeBSD VM testing.

## 4. Recommended order of work

1. Inspect CircleCI pipeline 1273 and establish why it is red.
2. Reconcile the CI-provider choice with the project's CircleCI decision; avoid launching duplicate full matrices.
3. Expand Windows from smoke coverage to the full applicable compiler regression suite.
4. Add explicit Windows and FreeBSD standard-library acceptance tests for every module included in stable 3.0.
5. Add FreeBSD HTTP/network integration coverage, including redirects and streaming if those APIs are included in stable 3.0.
6. Add sanitizer or platform-appropriate memory-check coverage and publish unsupported cases.
7. Resolve debugger support or explicitly mark it partial/unavailable on each platform.
8. Update smoke fixtures to the approved 3.0 declaration syntax after the parser implements it.
9. Require all platform gates to pass on the exact release candidate commit.

## 5. Release decision

**Windows: Partial — not yet eligible for an official full-support claim.**  
**FreeBSD: Partial — not yet eligible for an official full-support claim.**

These are not judgments that the existing ports are broken. They mean the current workflows do not yet prove the complete stable toolchain contract. Promote each target only after the gaps above are closed or explicitly scoped out of the platform's stable capability statement.

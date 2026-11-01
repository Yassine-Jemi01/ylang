# BSD platform support

BSD support is a YLang 3.0 development goal. It is being added incrementally;
the existence of a CI job is not, by itself, a stable-platform support claim.

## Current validation target

### FreeBSD

The first target is FreeBSD 15.x on x86-64.

The `.github/workflows/freebsd.yml` workflow boots a real FreeBSD virtual
machine and is intended to validate:

- Building the C17 compiler with the system Clang toolchain, including explicit `libm` linkage for BSD math symbols.
- Runtime-template generation and consistency checks.
- The compiler regression suite.
- The optional SDL2 GUI runtime and Game2D example build.
- Compiling and running a small native YLang executable.

The workflow is an initial compatibility gate. Its tasks remain validation
pending until the hosted FreeBSD job has completed successfully. It does not
yet establish complete parity with Linux or macOS.

## Planned targets

1. **FreeBSD:** establish the native compiler, regression, runtime, and SDL2
   baseline first.
2. **OpenBSD:** add a native VM job, then resolve differences in available
   system libraries, toolchain behavior, and runtime APIs.
3. **NetBSD:** add a native VM job and repeat the compiler/runtime acceptance
   suite.
4. **DragonFly BSD:** evaluate after the first three targets have a repeatable
   CI baseline.

## Support policy

A BSD target may be described as supported only when:

- Its compiler builds from a clean checkout using documented native tools.
- Its applicable regression, sanitizer, and runtime tests pass in native CI.
- Filesystem, process, environment, networking, and linker behavior are tested
  or explicitly documented as unavailable.
- Optional GUI dependencies have a separate tested capability status.
- The platform's limitations are visible in the support matrix and release
  documentation.

The NASM backend is currently Linux x86-64-specific. A successful FreeBSD C
backend build must not be interpreted as NASM portability.

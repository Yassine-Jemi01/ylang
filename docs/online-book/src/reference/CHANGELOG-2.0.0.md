# YLang 2.0.0 — Stable Release

YLang 2.0.0 establishes the stable compatibility contract for the documented core language and the default C-backend compiler workflow. This release keeps the language's explicit typing, source-located diagnostics, and inspectable C-generation pipeline as the supported foundation.

**Important compatibility boundary:** not every component introduced during the 2.0 development cycle is stable. The experimental subsystems listed below may change in future releases and are explicitly outside the stable-core compatibility guarantee.

## Stable core

- Explicit declarations, constants, fixed-size homogeneous arrays, scalar types, functions, and structured control flow.
- Explicit conversions and type checking, initialization checks, clear diagnostics, and safe rejection of unsupported syntax.
- The default `ylang build` path: validate `.yl` source, generate C, and invoke GCC or Clang for a native executable.
- Core strings, formatted strings, standard input, text file I/O, math helpers, path utilities, and checked runtime behavior documented by the core specification.
- The `check`, `build`, `emit-c`, and `fix` command-line workflows, with the limitations of each command documented in the README.

The stable language contract is defined in [the core language specification](language-spec.md). Where this release notes page and the reference differ, the specification and the explicit experimental labels in the relevant feature guide define the supported behavior.

## Experimental extensions

These features are available for evaluation, but are not covered by the stable-core compatibility promise:

- **Named imports and multi-file compilation.** No package registry, dependency solver, import aliases, or qualified module namespaces are promised.
- **Structs and enums.** Several advanced forms remain unsupported; consult [the struct reference](STRUCTS.md) and [the enum reference](ENUMS.md).
- **GUI and Game2D.** The optional SDL2-backed APIs are not a mature general-purpose game engine. Interactive window behavior requires a graphical session.
- **NASM backend.** Restricted to a documented Linux x86-64 subset; the C backend remains the default.
- **Native Windows support.** Experimental 64-bit MinGW-w64 path with focused smoke tests; MSVC and full feature parity are not claimed.
- **YSPM integration.** Optional Linux x86-64 `.yspkg` delivery for compatible YSPM systems. It is not a Windows/macOS installer and does not supply GCC/Clang or SDL2 development dependencies.

## Not implemented in 2.0.0

- `Option<T>` and `Result<T, E>` are not implemented; their design document is a proposal only.
- Classes/OOP, methods on structs, inheritance, generics, and a complete LLVM or independent native backend are not part of this release.
- Arrays cannot be returned or nested, and `char` is one byte rather than a Unicode scalar value.
- YLang does not claim Rust-level memory safety, sandboxing, or suitability for processing hostile source as a hardened security boundary.

## Build from source

Requirements are GCC or Clang with C17 support and GNU Make (`gmake` on macOS). For ordinary command-line programs, SDL2 is optional. GUI/Game2D builds additionally need SDL2 development files and `pkg-config`.

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
make test
./build/ylang --version
```

On macOS, use `gmake` for these build and test commands. Native Windows users should follow [the Windows guide](WINDOWS.md) and treat that path as experimental.

## Linux YSPM package

The stable release workflow builds the Linux x86-64 `.yspkg` from the exact tested release commit, inspects its contents, runs the packaged compiler, and attaches it only after every stable-release gate passes. Install it only on a compatible YSPM system. Consult [the YSPM integration guide](YSPM.md) for ABI, filesystem, and dependency details.

## Upgrade and bug reports

Projects moving from YLang 1.1.0 should first run their test suite with 2.0.0 and review experimental feature notes if using imports, structs, enums, GUI/Game2D, NASM, Windows, or YSPM integration.

When reporting a bug, include the exact version and commit, operating system, compiler, command line, a minimal reproducer, expected result, and actual result.

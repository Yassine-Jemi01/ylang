# YLang 2.0 Beta — Release Notes

The YLang 2.0 beta line is experimental; stable 1.1.0 remains the recommended choice for production use of the documented stable subset. The already-published `v2.0.0-beta.1` remains source-only. A future prerelease may additionally attach a Linux YSPM `.yspkg` after the package build passes CI; existing release tags are never overwritten.

## Highlights

- **Named imports and multi-file compilation:** import named functions and global variables from relative source files, with diagnostics for missing modules or symbols and duplicate imports.
- **Typed enums and exhaustive matching (experimental):** unit enum variants, enum values in variables and function signatures, type checks, and exhaustive `match` statements.
- **Typed structs (experimental):** positional construction, field access, passing and returning struct values, and type-checked mutable field assignment.
- **Optional SDL2 GUI runtime:** window creation, event polling, drawing primitives, and cleanup APIs.
- **2D game helpers (experimental):** drawing, input, timing, and collision helpers built around the optional GUI runtime; a small playable Dodge sample demonstrates how to make a simple game.
- **YSPM Linux package integration (experimental):** CI can build a native `.yspkg` containing the YLang compiler and standard-library sources for a compatible Linux x86-64 YSPM system. YSPM manages system packages; it does not resolve project-local `.yl` modules.
- **Native Windows foundation (experimental):** MinGW-w64 x86-64 CI smoke tests cover compiler build, native executable generation, relative imports, standard input, and SDL2 runtime build.
- **Improved compiler validation:** regression coverage, frontend unit tests, a lexer/parser fuzz smoke test, sanitizers, and editor-diagnostic tests in CI.

## Backend status

- The **C backend remains the default** for `ylang build`.
- The NASM backend is experimental and supports only a documented subset. Do not assume that every C-backend program can be compiled to NASM.
- GUI/game programs require SDL2 development dependencies and a graphical display to run.
- Native Windows x86-64 support through MinGW-w64 is experimental. A focused Windows CI smoke job covers build/run, imports, input, and SDL2 linking; this is not yet full feature parity. See `docs/WINDOWS.md`.

## Known limitations

- `Option<T>` and `Result<T, E>` are **not implemented**. The design document is a proposal, not working language syntax.
- Enums currently have no payload variants, explicit discriminants, or enum formatting. Enum imports across module boundaries are not promised.
- Structs do not provide methods, inheritance, generics, field defaults, or named constructor arguments; recursive by-value fields are unsupported.
- The import system does not yet provide import aliases, qualified module namespaces, or circular imports. There is no official project-level `.yl` package registry or dependency solver in this release; YSPM is not intended to provide that functionality.
- The 2D/GUI APIs are experimental and should not be treated as a mature game engine.
- This beta does not claim Rust-level memory safety or production stability.

## Installation and artifacts

The published `v2.0.0-beta.1` tag is source-only. For a future beta, the release workflow is being extended to attach a Linux x86-64 YSPM `.yspkg` built from the exact tested commit. This package is optional and does not imply native Windows/macOS installers.

For GUI/game examples, install SDL2 development files and `pkg-config`. Linux and macOS are CI-tested; native Windows support uses MinGW-w64 x86-64 and remains experimental. See `docs/WINDOWS.md`. The YSPM build and install workflow is documented in `docs/YSPM.md`.

## Upgrade and bug reports

- Keep **1.1.0** for projects that need the stable documented subset.
- Test beta code in a separate branch or checkout before migrating projects.
- When reporting a bug, include the exact version/commit, operating system, compiler, command, a minimal reproducer, expected result, and actual result.

The `2.0.0-beta.1` version is a prerelease for evaluation and feedback. Its behavior and feature coverage may change before a stable 2.0.0 release.

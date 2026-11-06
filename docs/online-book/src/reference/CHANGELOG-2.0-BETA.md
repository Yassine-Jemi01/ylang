# YLang 2.0.0-beta.2 — Release Notes

YLang 2.0.0-beta.2 is an experimental prerelease, not a stable release. The previously published `v2.0.0-beta.1` remains source-only and unchanged. This beta adds the Linux x86-64 YSPM package artifact through a release workflow that rebuilds and checks the package on the exact candidate commit. Stable 1.1.0 remains the recommended line for projects relying on the documented stable subset.

## Highlights

- **Named imports and multi-file compilation:** import named functions and global variables from relative source files, with diagnostics for missing modules or symbols and duplicate imports.
- **Typed enums and exhaustive matching (experimental):** unit enum variants, enum values in variables and function signatures, type checks, and exhaustive `match` statements.
- **Typed structs (experimental):** positional construction, field access, passing and returning struct values, and type-checked mutable field assignment.
- **Optional SDL2 GUI runtime:** window creation, event polling, drawing primitives, and cleanup APIs.
- **2D game helpers (experimental):** drawing, input, timing, and collision helpers around the optional GUI runtime; the playable Dodge sample demonstrates a small game.
- **YSPM Linux package integration (experimental):** this prerelease includes a Linux x86-64 `.yspkg` built from the release candidate for compatible YSPM systems. YSPM manages system packages; it does not resolve project-local `.yl` modules.
- **Native Windows foundation (experimental):** MinGW-w64 x86-64 CI smoke tests cover compiler build, native executable generation, relative imports, standard input, and SDL2 runtime build.
- **Improved compiler validation:** regression coverage, frontend unit tests, a lexer/parser fuzz smoke test, sanitizers, and editor-diagnostic tests in CI.

## Backend status

- The **C backend remains the default** for `ylang build`.
- The NASM backend is experimental and supports only a documented subset. Do not assume that every C-backend program can be compiled to NASM.
- GUI/game programs require SDL2 development dependencies and a graphical display to run.
- Native Windows x86-64 support through MinGW-w64 is experimental. A focused Windows CI smoke job covers build/run, imports, input, and SDL2 linking; this is not full feature parity. See `docs/WINDOWS.md`.

## Known limitations

- `Option<T>` and `Result<T, E>` are **not implemented**. The design document is a proposal, not working language syntax.
- Enums currently have no payload variants, explicit discriminants, or enum formatting. Enum imports across module boundaries are not promised.
- Structs do not provide methods, inheritance, generics, field defaults, or named constructor arguments; recursive by-value fields are unsupported.
- The import system does not yet provide import aliases, qualified module namespaces, or circular imports. There is no official project-level `.yl` package registry or dependency solver in this release; YSPM is not intended to provide that functionality.
- The 2D/GUI APIs are experimental and should not be treated as a mature game engine.
- Object-oriented programming and a complete LLVM/independent native backend are intentionally deferred to later releases.
- This beta does not claim Rust-level memory safety or production stability.

## Installation and artifacts

The release workflow builds the Linux x86-64 YSPM `.yspkg` on the exact candidate commit, validates package metadata and required files, starts the packaged compiler, and attaches the artifact to the prerelease. Install it on a compatible YSPM system using:

```sh
sudo yspm install ./ylang-2.0.0-beta.2-x86_64.yspkg
```

The package is an optional Linux system-package route; it is not a Windows or macOS installer. For source builds on Linux/macOS and the experimental MinGW-w64 Windows path, see `README.md` and `docs/WINDOWS.md`.

For GUI/game examples, install SDL2 development files and `pkg-config`. The CI validates compilation and linking, but live window interaction needs a graphical session and is not tested by headless CI. See `docs/GAME2D.md` and `docs/YSPM.md`.

## Upgrade and bug reports

- Keep **1.1.0** for projects that need the stable documented subset.
- Test beta code in a separate branch or checkout before migrating projects.
- When reporting a bug, include the exact version/commit, operating system, compiler, command, a minimal reproducer, expected result, and actual result.

The `2.0.0-beta.2` version is for evaluation and feedback. Behavior and feature coverage may change before a stable 2.0.0 release.

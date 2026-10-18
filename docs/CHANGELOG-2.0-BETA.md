# YLang 2.0.0-beta.1 — Beta Release Notes

YLang 2.0.0-beta.1 is an early, source-only prerelease for testing the expanded compiler and standard library. The latest stable release remains **1.1.0**. This beta is not intended for production-critical software.

## Highlights

- **Named imports and multi-file compilation:** import named functions and global variables from relative source files, with diagnostics for missing modules or symbols and duplicate imports.
- **Typed enums and exhaustive matching (experimental):** unit enum variants, enum values in variables and function signatures, type checks, and exhaustive `match` statements.
- **Typed structs (experimental):** positional construction, field access, passing and returning struct values, and type-checked mutable field assignment.
- **Optional SDL2 GUI runtime:** window creation, event polling, drawing primitives, and cleanup APIs.
- **2D game helpers (experimental):** drawing, input, timing, and collision helpers built around the optional GUI runtime, with a playable Dodge sample and an API guide.
- **Improved compiler validation:** regression coverage, frontend unit tests, a lexer/parser fuzz smoke test, sanitizers, and editor-diagnostic tests in CI.

## Backend status

- The **C backend remains the default** for `ylang build`.
- The NASM backend is experimental and supports only a documented subset. Do not assume that every C-backend program can be compiled to NASM.
- GUI/game programs require SDL2 development dependencies and a graphical display to run.
- The compiler driver currently relies on POSIX facilities; native Windows builds are not supported.

## Known limitations

- `Option<T>` and `Result<T, E>` are **not implemented**. The design document is a proposal, not working language syntax.
- Enums currently have no payload variants, explicit discriminants, or enum formatting. Enum imports across module boundaries are not promised.
- Structs do not provide methods, inheritance, generics, field defaults, or named constructor arguments; recursive by-value fields are unsupported.
- The module system does not yet provide package manifests, import aliases, qualified module namespaces, circular imports, or a package registry.
- The 2D/GUI APIs are experimental and should not be treated as a mature game engine.
- This beta does not claim Rust-level memory safety or production stability.

## Installation and artifacts

This release is source-only. GitHub provides source archives for the tagged commit; **no precompiled binaries or installers are included**. Build the compiler from source using the prerequisites in the repository README.

For GUI examples, also install SDL2 development files and `pkg-config`. The documented targets are Linux and macOS.

## Upgrade and bug reports

- Keep **1.1.0** for projects that need the stable documented subset.
- Test beta code in a separate branch or checkout before migrating projects.
- When reporting a bug, include the exact version/commit, operating system, compiler, command, a minimal reproducer, expected result, and actual result.

The `2.0.0-beta.1` version is a prerelease for evaluation and feedback. Its behavior and feature coverage may change before a stable 2.0.0 release.

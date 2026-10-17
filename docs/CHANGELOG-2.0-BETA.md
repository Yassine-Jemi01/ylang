# YLang 2.0.0-beta.1 — draft release notes

> **Status: draft, not a published release.** These notes describe the current development snapshot and must be reconciled with the exact release-candidate commit before tagging.

## Highlights

- **Named imports and multi-file compilation:** import named functions and global variables from relative source files, with diagnostics for missing modules or symbols and duplicate imports.
- **Typed enums and exhaustive matching (experimental):** unit enum variants, enum values in variables and function signatures, type checks, and exhaustive `match` statements.
- **Typed structs (experimental):** positional construction, field access, passing and returning struct values, and type-checked mutable field assignment.
- **Optional SDL2 GUI runtime:** window creation, event polling, drawing primitives, and cleanup APIs.
- **2D game helpers (experimental):** drawing/input/timing/collision helpers built around the optional GUI runtime.
- **Improved regression coverage:** CI exercises GCC and Clang on Linux, Clang on macOS, sanitizers, and editor diagnostic parsers.

## Backend status

- The **C backend remains the default** for `ylang build`.
- The NASM backend is experimental and supports only a documented subset. Do not assume that all C-backend programs can be compiled to NASM.
- GUI/game programs require the optional SDL2 development dependencies and a graphical display to run.
- The compiler driver currently relies on POSIX facilities; native Windows builds are not supported.

## Not included / known limitations

- `Option<T>` and `Result<T, E>` are **not implemented**. The design document is a proposal, not working language syntax.
- Enums currently have no payload variants, explicit discriminants, or enum formatting; enum imports across module boundaries are not promised.
- Structs do not provide methods, inheritance, generics, field defaults, or named constructor arguments; recursive by-value fields are unsupported.
- The module system does not yet provide package manifests, import aliases, qualified module namespaces, circular imports, or a package registry.
- The 2D/GUI APIs are experimental and should not be treated as a mature game engine.
- This beta does not claim Rust-level memory safety or production stability.

## Upgrade and testing notes

- Keep the stable **1.1.0** release for projects that need the documented stable subset.
- Test this beta from a clean checkout and follow the current README build prerequisites.
- When reporting a bug, include the exact commit/version, OS, compiler, command, a minimal reproducer, expected output, and actual output.

## Publication gate

Before publishing, replace this draft with the final release-candidate commit hash, verify every claim against the tagged source, run the complete CI matrix on that exact commit, and publish `2.0.0-beta.1` as a GitHub **prerelease**. Do not call it stable.

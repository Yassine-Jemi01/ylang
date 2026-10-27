# The YLang Book

Welcome to **The YLang Book**, the guided online guide to the YLang programming language and compiler.

This is a real multi-page HTML book—not a raw Markdown page or a list of links. Use the chapter sidebar to move through the material, search the book, switch its appearance, copy code examples, or move to the previous and next chapter. Reference chapters are generated from the canonical files in `docs/`, so the language specification and implementation guides remain maintained in one place.

## Choose the right version

- **Latest stable release — 2.2.0:** the documented core language and default C-backend workflow remain the compatibility foundation.
- **2.2.0 additions:** experimental HTTP/async-task APIs, environment-variable access, a terminal-based source-aware debugger, and a reproducible [performance harness](reference/PERFORMANCE.md). Read the [security model](reference/SECURITY.md) before using network APIs in a server process.
- Read the [YLang 2.2.0 release notes](reference/CHANGELOG-2.2.0.md) for new capabilities and known limitations.

Named imports, structs, enums, GUI/Game2D, NASM, native Windows support, and optional YSPM packaging remain experimental extensions with explicit limits. They are not covered by the stable-core compatibility guarantee; YLang does not claim Rust-level memory safety or provide a security sandbox.

## Start reading

- Evaluate the [experimental memory model](reference/MEMORY-MODEL.md) and read the [2.1.0 release-candidate notes](reference/CHANGELOG-2.1.0.md) before using ownership APIs. The candidate is not a published release.

- [Install YLang](getting-started.md) and set up a compiler toolchain.
- Build and run [your first program](hello-world.md).
- Learn the [compiler workflow](compiler-workflow.md) and [language fundamentals](language-basics.md).
- Explore [types and explicit conversions](types.md), then [functions and modules](modules.md).
- Build a small graphical project with [GUI and Game2D](gui-game2d.md).

For a print-friendly edition, open the [existing PDF book](../YLang-Book.pdf). The PDF is an earlier edition; use this online book and the current language specification for YLang 2.2.0. For source, issues, and release downloads, visit the [YLang GitHub repository](https://github.com/Yassine-Jemi01/ylang).

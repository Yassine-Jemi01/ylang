# The YLang Book

Welcome to **The YLang Book**, the guided online guide to the YLang programming language and compiler.

This is a real multi-page HTML book—not a raw Markdown page or a list of links. Use the chapter sidebar to move through the material, search the book, switch its appearance, copy code examples, or move to the previous and next chapter. Reference chapters are generated from the canonical files in `docs/`, so the language specification and implementation guides remain maintained in one place.

## Choose the right version

- **Stable core — 2.0.1:** the documented core language and default C-backend workflow.
- **Previous stable release — 2.0.0:** useful for projects that pin the original 2.0 stable patch.
- **Earlier stable line — 1.1.0:** useful for older projects that intentionally stay on that contract.

Named imports, structs, enums, GUI/Game2D, NASM, native Windows support, and optional YSPM packaging remain experimental extensions with explicit limits. They are not covered by the stable-core compatibility guarantee; YLang does not claim Rust-level memory safety or provide a security sandbox.

## Start reading

- [Install YLang](getting-started.md) and set up a compiler toolchain.
- Build and run [your first program](hello-world.md).
- Learn the [compiler workflow](compiler-workflow.md) and [language fundamentals](language-basics.md).
- Explore [types and explicit conversions](types.md), then [functions and modules](modules.md).
- Build a small graphical project with [GUI and Game2D](gui-game2d.md).

For a print-friendly edition, open the [existing PDF book](../YLang-Book.pdf). The PDF is an earlier edition; use this online book and the current language specification for the 2.0.1 release. For source, issues, and release downloads, visit the [YLang GitHub repository](https://github.com/Yassine-Jemi01/ylang).

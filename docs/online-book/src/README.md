# The YLang Book

Welcome to **The YLang Book**, the official online guide to the YLang programming language, its compiler, and its tools.

This book is organized as a guided course and a technical reference. You can read it from the beginning to learn the language step by step, or use the chapter sidebar and search to jump directly to a feature. Code examples are copyable, the appearance can be changed, and previous/next navigation helps you move through the chapters.

The online edition is the canonical book. It is maintained alongside the compiler and its reference documents, so corrections can be published without maintaining a separate PDF edition. The tutorial explains how to use YLang; the reference chapters describe exact syntax, supported behavior, and known restrictions.

## Which version does this book describe?

This edition documents **YLang 2.2.0**, the current stable release at the time of writing. The project is also developing YLang 2.3.0; its [development notes](reference/CHANGELOG-2.3.0.md), [release gates](reference/ROADMAP-2.3.0.md), and [pre-release checklist](reference/RELEASE-CHECKLIST-2.3.0.md) describe unreleased work and do not change the stable version.

The stable core uses explicit types and the default C-backend workflow. Some features added around the core are experimental and have narrower support:

- HTTP requests and environment-variable access have documented runtime and security limits.
- The source-aware terminal debugger and performance harness are tools for development and investigation.
- Structs, enums, named imports, GUI/Game2D, the NASM backend, native Windows support, and YSPM packaging have feature-specific restrictions.
- The NASM backend does not support every program accepted by the C backend.
- YLang does not claim Rust-level memory safety and does not provide a security sandbox.
- `Option<T>` and `Result<T, E>` remain design proposals, not supported language constructs.

Always check the relevant reference chapter before depending on an experimental feature in a project. A construct being accepted by one compiler stage does not guarantee that every backend supports it.

## A recommended reading path

1. Read the [Introduction](introduction.md) to understand YLang's goals and support boundaries.
2. [Install YLang](getting-started.md) and verify that the compiler and C toolchain work.
3. Write, check, build, and run [your first program](hello-world.md).
4. Learn the [compiler workflow](compiler-workflow.md) so you can distinguish source checking, code generation, and native compilation.
5. Practice by repairing small programs with [YLanglings](ylanglings.md), the offline compiler-backed exercise track.
5. Study [program structure](language-basics.md), [types and conversions](types.md), and [functions and modules](modules.md).
7. Explore the [standard library](standard-library.md), then use the reference chapters when you need exact details.
8. Try [GUI and Game2D](gui-game2d.md) only after the command-line workflow is working.

## How to use this book

- **Tutorial chapters** introduce a topic, explain why it matters, and walk through small examples.
- **Reference chapters** document syntax, contracts, error codes, platform requirements, and limitations.
- **Release notes and roadmaps** describe a particular release or proposal. A design document is not proof that a feature has shipped.
- **Compiler diagnostics** should be read with their complete error code and message; use the [Compiler Error Codes](reference/error-codes.md) chapter when investigating a failure.

For source code, issue reports, and release downloads, visit the [YLang GitHub repository](https://github.com/Yassine-Jemi01/ylang).

The book is continuously maintained with the project. If an example appears to disagree with the compiler, check the canonical [language specification](reference/language-spec.md) and report the mismatch with a minimal reproducible example.
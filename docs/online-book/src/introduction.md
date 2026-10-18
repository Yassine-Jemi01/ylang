# Introduction

YLang is a small compiled programming language with explicit types and actionable compiler diagnostics. Its compiler is written in C17. The default build path checks a `.yl` program, emits C, and invokes GCC or Clang to create a native executable.

This book combines a progressive tutorial with the project's detailed reference documents. The tutorial chapters help you get a program running; the reference chapters describe the stable core, diagnostics, platform requirements, and the exact limits of experimental extensions.

## What YLang is—and is not

YLang is compiled, not an interpreter. The compiler checks syntax, names, initialization, and types before generating and compiling the program. The C backend remains the broad-coverage default. An experimental NASM backend supports only a restricted Linux x86-64 subset.

YLang does not provide every feature of a mature systems language. `Option<T>` and `Result<T, E>` are not implemented. Structs, enums, imports, GUI/Game2D APIs, the NASM backend, and native Windows support remain experimental extensions. YLang makes no Rust-level memory-safety claim.

## Stable and experimental material

Throughout the book, the stable 2.0.0 core is distinguished from experimental extensions. Platform and feature pages state which paths are tested and which remain experimental. Do not infer that a syntax feature works in every backend just because the parser accepts it; consult the relevant reference chapter.

Continue to [install YLang](getting-started.md), or open the [complete language specification](reference/language-spec.md) when you need an exact syntax and behavior reference.

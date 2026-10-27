# Introduction

YLang is a compiled programming language designed around readable source code, explicit types, and compiler diagnostics that help you locate mistakes. The compiler is written in C17. Its default workflow checks a \`.yl\` source file, translates it to C, and invokes GCC or Clang to produce a native executable.

This book is both a tutorial and a technical reference. You will start with a small command-line program, learn how declarations and functions are organized, and then explore the compiler, standard library, optional graphical helpers, and platform-specific workflows.

## What YLang is

YLang source code is written in \`.yl\` files. A program describes values and operations using the language's own syntax. Before a native executable is produced, the compiler checks the source and reports issues such as syntax errors, unknown names, invalid assignments, and type mismatches.

The default backend generates C, so a compatible C compiler is part of the normal build process. GCC and Clang are the supported choices exposed by the command-line interface. Generated C can also be emitted for inspection when debugging a compiler issue or learning about the compilation process.

YLang also contains experimental extensions and tools. These include a restricted NASM backend, structs and enums, imports, GUI/Game2D helpers, HTTP and environment APIs, a terminal debugger, and optional Linux packaging. Each extension has its own support boundaries; the existence of a feature does not mean it is equally supported by every backend or platform.

## What YLang is not

YLang is not an interpreted language, and the default compiler workflow is not an independent machine-code backend: it relies on GCC or Clang after generating C.

YLang is also not a complete systems language with every advanced feature available. In particular:

- \`Option<T>\` and \`Result<T, E>\` are design proposals, not current language constructs.
- The experimental NASM backend supports a narrower subset than the C backend.
- GUI/Game2D requires additional native dependencies and a graphical session for live-window testing.
- Native Windows support is experimental and does not promise complete feature parity.
- YLang does not claim Rust-level memory safety or provide a security sandbox.

These distinctions are important when deciding whether a feature is appropriate for a real project. This book labels experimental functionality and links to its contract or design document wherever possible.

## Stable core and experimental extensions

YLang 2.2.0 is the release described by this edition. The stable core and its default C-backend workflow form the compatibility baseline. Extensions may change or have restrictions that do not apply to the stable core.

When choosing a feature, follow this checklist:

1. Read the relevant tutorial to understand its intended use.
2. Open the corresponding reference or specification for exact syntax.
3. Check platform and backend support.
4. Build and run a small example in the same environment you intend to use.
5. Keep a minimal reproducer if the compiler behaves differently than expected.

Do not infer runtime behavior from syntax alone. A program that passes one stage of compilation may still fail during native compilation, linking, or execution.

## How to read examples

Most tutorial examples are small on purpose. They isolate one idea so that you can change a value, run the compiler again, and observe what changes. Unless a chapter says otherwise, commands assume that you are in the root of the cloned YLang repository and that the compiler has already been built at \`build/ylang\`.

Shell commands and YLang source serve different purposes:

- Shell commands such as \`make\` build the compiler or invoke it.
- YLang code belongs in a \`.yl\` source file.
- Output shown after running a program is an example of the program's behavior, not another command to type.

If you see a mismatch between an example and the implementation, use the [Complete Language Specification](reference/language-spec.md) and report the smallest example that reproduces the problem.

## Where to go next

If you have not built the compiler yet, continue to [Install YLang](getting-started.md). If you already have a working build, go straight to [Your First Program](hello-world.md). For an exact list of accepted syntax, keep the [Complete Language Specification](reference/language-spec.md) nearby.
# YLanglings — Practice by fixing code

YLanglings is an optional, offline practice tool that complements this book.
Instead of only reading examples, you receive a small YLang program with an
intentional mistake. Read the compiler diagnostic, edit the source, and verify
your fix with the real YLang compiler.

## Start locally

From the YLang repository root:

```sh
python3 tools/ylanglings/ylanglings.py list
python3 tools/ylanglings/ylanglings.py next
python3 tools/ylanglings/ylanglings.py verify 01
```

If the compiler is not on PATH, pass its location:

```sh
python3 tools/ylanglings/ylanglings.py --compiler ./build/ylang verify 01
```

## Learning path

The core path starts with program structure, types, constants and mutable
variables, then moves through conditions, loops, functions, arrays, structs,
enums and exhaustive matching, strings and f-strings, explicit conversions,
modules, and text file I/O. The optional track introduces JSON builders,
Binary I/O, HTTP Streaming, terminal UI, Voice, GUI, and owned-string APIs.
The exercise collection is designed to grow with the language and its
standard library.

Optional challenges are separate from the core path. They can require combining
several concepts or solving a problem with less guidance. They are never needed
to finish the core track.

## How verification works

YLanglings invokes the installed compiler's `check` and `build` commands,
runs the resulting program with a short time limit, and compares its output
with the expected result. Progress is saved locally. The tool does not need an
internet connection and does not send telemetry.

YLanglings is a learning aid, not a replacement for the compiler test suite,
security review, or documentation. It only verifies the behavior specified by
each exercise.

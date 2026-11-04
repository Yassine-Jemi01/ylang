# YLanglings — Practice by fixing code

YLanglings is YLang's offline, compiler-backed learning tool, inspired by
Rustlings. Instead of only reading examples, you receive a small YLang program
with an intentional mistake. Read the compiler diagnostic, edit the source,
and verify your fix with the real YLang compiler.

YLanglings is part of the YLang 2.3 developer-experience scope. It is a
companion tool rather than a compiler dependency.

## Start locally

From the YLang repository root, install the local command:

```sh
make install-ylanglings
```

Then use it from any directory:

```sh
ylanglings list
ylanglings next
# After finishing the core track:
ylanglings next --optional
ylanglings run 01
ylanglings edit 01
```

Edit the displayed source file, then open it with `ylanglings edit 01` if desired and verify it:

```sh
ylanglings verify 01
```

If the compiler is not on PATH, pass its location:

```sh
ylanglings --compiler ./build/ylang verify 01
```

Use `ylanglings hint NUMBER` for a hint and `ylanglings reset NUMBER` to
restore an exercise's original broken starter. The `list` command shows
progress, while `next` shows the first incomplete core exercise. Use `next --optional` to move through optional challenges. Use `edit NUMBER` to open it in your configured editor.

## Learning path

The core track contains 16 exercises. It starts with program structure,
variables and types, and mutability, then moves through conditions, loops,
functions, arrays, structs, enums, strings, modules, f-string interpolation,
explicit conversions, loop control, and text file I/O.

The optional track contains 10 challenges for nested loops, enum logic,
struct functions, JSON, Binary I/O, HTTP Streaming, terminal UI, Voice, GUI
lifecycle, and owned strings. Optional challenges are separate from core
completion and are never required to finish the core track.

## How verification works

For runnable exercises, YLanglings invokes the compiler's `check` and
`build` commands, runs the resulting program with a ten-second limit, and
compares its output with the expected result. Progress is saved locally using
an atomic file replacement. The tool does not need an internet connection,
does not install third-party Python packages, and sends no telemetry.

Compiler check/build subprocesses have a 60-second timeout. Exercise programs are native executables and run with the current user's permissions; YLanglings is not a sandbox, so review source before running it.

Some native API exercises are intentionally marked compile-only. They verify
that the compiler accepts the documented API signatures; they do not pretend
to exercise a native audio, GUI, or terminal device. Runtime behavior for
those integrations belongs to the compiler's dedicated integration tests.

YLanglings is a learning aid, not a replacement for the compiler regression
suite, security review, or sanitizer tests. It verifies only the behavior
specified by each exercise.

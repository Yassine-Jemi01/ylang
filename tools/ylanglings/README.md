# YLanglings

YLanglings is YLang's offline, compiler-backed learning track, inspired by Rustlings.
It ships intentionally broken YLang source files. Learners edit each exercise,
then use the real YLang compiler to check, build, and run their solution.

YLanglings is included in the YLang 2.3 developer experience and release review.
It is a companion tool rather than a compiler dependency.

## Requirements

- Python 3.10+
- A built YLang compiler available as `ylang` on PATH (or pass `--compiler`)
- No network connection and no third-party Python packages

## Start

Install it for the current user:

```sh
make -C tools/ylanglings install
```

Ensure `~/.local/bin` is on PATH and that the YLang compiler is available. Then:

```sh
ylanglings list
ylanglings next
ylanglings run 01
```

Edit the displayed `.yl` file, then verify it:

```sh
ylanglings verify 01
```

If YLang is not on PATH:

```sh
ylanglings --compiler ./build/ylang verify 01
```

Other commands:

- `hint NUMBER` — show a hint without revealing the solution.
- `reset NUMBER` — restore the original broken starter.
- `list` — show core and optional exercise progress.
- `next` — open the first incomplete core exercise.

To uninstall:

```sh
make -C tools/ylanglings uninstall
```

Progress is stored locally in `$XDG_CONFIG_HOME/ylanglings/progress.json`
(or `~/.config/ylanglings/progress.json`). Exercise source files remain in
the installation directory, so they can be edited with any editor. No telemetry
is sent and no network access is required.

## Exercise tracks

The core track contains 16 exercises covering syntax, values and types,
mutability, conditions, loops, functions, arrays, structs, enums, strings,
modules, conversions, loop control, and file I/O.

The optional track contains 10 challenges for nested loops, enum logic,
struct functions, JSON, Binary I/O, HTTP streaming, TUI, Voice, GUI lifecycle,
and owned strings. Optional challenges are separate from core completion.

## Verification model

For runnable exercises, verification requires the source to pass `ylang check`,
build successfully, exit with status zero, and produce the exact expected output.
Each program is limited to ten seconds. API/signature exercises marked
`mode: check` are compiler-checked without launching native integrations;
their runtime behavior is covered by the compiler's integration suite.

YLanglings does not claim to validate memory safety or replace the compiler's
full regression and sanitizer suites.

## Adding an exercise

Add a broken starter under `exercises/` and a manifest entry containing:

- `title`, `category`, `file`, and `starter`
- a short `description` and a non-spoiling `hint`
- `expected_output` for runnable exercises
- `mode: check` only when the exercise is intentionally a compile-time API check
- `optional: true` for challenges outside the core track

Keep starter files small and focused on one learning objective. The
YLanglings manifest and CLI contract are checked in CI.

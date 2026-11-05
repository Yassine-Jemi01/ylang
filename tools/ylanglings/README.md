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
- Windows installation commands use the MSYS2 MINGW64 shell; the Python CLI itself is platform-independent

## Start

Install it for the current user (the default path is `$HOME/.local`):

```sh
make install-ylanglings PREFIX="$HOME/.local"
```

Ensure `~/.local/bin` is on PATH and that the YLang compiler is available. Then:

```sh
ylanglings list
ylanglings next
# After finishing the core track:
ylanglings next --optional
ylanglings run 01
ylanglings edit 01
```

Edit the displayed `.yl` file directly, or use `ylanglings edit 01` to open it in your configured editor; then verify it:

```sh
ylanglings verify 01
```

If YLang is not on PATH:

```sh
ylanglings --compiler ./build/ylang verify 01
```

Other commands:

- `verify-all` — maintainer-only command available from a YLanglings source checkout. It checks/builds/runs all 26 reference solutions, collecting failures instead of stopping at the first one. It never writes learner progress. It is not included in the user installation because reference solutions are deliberately kept out of the learner package.
- `hint NUMBER` — show a hint without revealing the solution.
- `reset NUMBER` — restore the original broken starter.
- `list` — show core and optional exercise progress.
- `next` — show the first incomplete core exercise; use `next --optional` for the optional track.
- `edit NUMBER` — open an exercise in the editor selected by `VISUAL` or `EDITOR`.

To uninstall:

```sh
make uninstall-ylanglings PREFIX="$HOME/.local"
```

The installer marks the data directory it creates. Installation and removal refuse unmanaged or symlinked data directories rather than overwriting or recursively deleting them.

Progress is stored locally in `$XDG_CONFIG_HOME/ylanglings/progress.json`
(or `~/.config/ylanglings/progress.json`). Exercise source files remain in
the installation directory, so they can be edited with any editor. No telemetry
is sent and no network access is required.

## Exercise tracks

The core track contains 16 exercises covering syntax, values and types,
mutability, conditions, loops, functions, arrays, structs, enums, strings,
modules, conversions, loop control, and file I/O.

The optional track contains 10 challenges for nested loops, enum logic,
struct functions, JSON, Binary I/O, HTTP streaming, TUI, Voice, JSON builders,
and owned strings. Optional challenges are separate from core completion.

## Verification model

For runnable exercises, verification requires the source to pass `ylang check`,
build successfully, exit with status zero, and produce the exact expected output.
Each compiler check/build command has a 60-second limit, and each exercise
program is limited to ten seconds. API/signature exercises marked
`mode: check` are compiler-checked without launching native integrations;
their runtime behavior is covered by the compiler's integration suite.

Exercise programs are native executables and run with the current user's permissions; YLanglings is not a sandbox. Review a starter and your changes before running them. YLanglings does not claim to validate memory safety or replace the compiler's full regression and sanitizer suites.

## Adding an exercise

Add a broken starter under `exercises/` and a manifest entry containing:

- `title`, `category`, `file`, `starter`, and a unique `solution` path
- a short `description` and a non-spoiling `hint`
- a private maintainer reference file under `solutions/`; it is never copied into learner installs
- `expected_output` for runnable exercises
- `mode: check` only when the exercise is intentionally a compile-time API check
- `optional: true` for challenges outside the core track

Keep starter files small and focused on one learning objective. The 2.3
manifest validator intentionally pins the curriculum at 16 core and 10 optional
exercises; changing those counts requires updating the validator, contract tests,
and release acceptance checklist in the same change. The YLanglings manifest
and CLI contract are checked in CI.

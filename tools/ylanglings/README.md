# YLanglings

YLanglings is an offline, compiler-backed practice tool inspired by Rustlings.
It ships intentionally broken YLang source files. The learner edits the source,
then asks the real YLang compiler to check, build, and run the solution.

It is an optional learning companion, not a compiler dependency and not a
release blocker for YLang 2.3.

## Requirements

- Python 3.10+
- A built YLang compiler available as \`ylang\` on PATH (or pass \`--compiler\`)
- No network connection and no third-party Python packages

## Start

Install it for the current user (no network or package manager required):

\`\`\`sh
make -C tools/ylanglings install
\`\`\`

Ensure \`~/.local/bin\` is on PATH and that the YLang compiler is available. Then start:

\`\`\`sh
ylanglings list
ylanglings next
ylanglings run 01
\`\`\`

To uninstall:

\`\`\`sh
make -C tools/ylanglings uninstall
\`\`\`

Edit the displayed \`.yl\` file, then verify it:

\`\`\`sh
python3 tools/ylanglings/ylanglings.py verify 01
\`\`\`

If YLang is not on PATH:

\`\`\`sh
python3 tools/ylanglings/ylanglings.py --compiler ./build/ylang verify 01
\`\`\`

Other commands:

- \`hint NUMBER\` — show a hint, not the solution.
- \`reset NUMBER\` — restore the original broken starter.
- \`list\` — show progress.
- \`next\` — open the first incomplete exercise.

Progress is stored locally in \`$XDG_CONFIG_HOME/ylanglings/progress.json\`
(or \`~/.config/ylanglings/progress.json\`). Exercise source files remain in
this directory, so they can be edited with any editor. No telemetry is sent.

## Exercise design

The core track covers syntax, values and types, mutability, expressions,
conditions, loops, functions, arrays, structs, enums, modules, strings,
diagnostics, and standard-library use. Optional challenges are marked
\`optional\` in the manifest and are never required to finish the core track.

Verification is deliberately stricter than \`ylang check\`: the source must
also build, exit successfully, and produce the expected output. Each run is
limited to ten seconds. The exercise tool does not claim to validate memory
safety or replace the compiler's full test suite.

## Adding an exercise

Add a broken starter file under \`exercises/\` and a manifest entry containing:

- \`title\`, \`category\`, \`file\`, and \`starter\`
- a short \`description\` and a non-spoiling \`hint\`
- \`expected_output\` for the corrected program

Keep starter files intentionally small, with one main learning objective.

# Functions and Modules

Functions provide named units of behavior; imports let a source file use selected declarations from another \`.yl\` file. This chapter explains how to keep code organized as a project grows from one file to several.

## Functions first

A function should have a clear purpose, a readable signature, and a body that performs a related set of operations. Parameters describe the values the caller supplies; the return type describes the value produced by the function.

\`\`\`ylang
fn answer() -> int {
    return 42;
}

fn main() -> int {
    print(answer());
    return 0;
}
\`\`\`

The caller uses the function's name and parentheses to invoke it. If a function signature changes, check every caller for compatible arguments and return-value handling.

## A local import

Given a sibling file named \`math_tools.yl\`:

\`\`\`ylang
fn answer() -> int {
    return 42;
}
\`\`\`

A second file can import that function:

\`\`\`ylang
from "./math_tools.yl" import answer;

fn main() -> int {
    print(answer());
    return 0;
}
\`\`\`

Relative paths are resolved from the importing source file. The imported name becomes available to the importing source according to the module system's current rules.

In this release, imports are experimental and do not imply a central package registry or dependency solver. Import aliases and qualified module namespaces are not part of the current contract.

## Organize one module at a time

Keep each module focused on one coherent responsibility. A small math module, for example, can hold reusable calculations; the entry-point file can then coordinate input, calls, and output.

A practical workflow is:

1. Create the new source file and add one small function.
2. Import only the declarations the caller needs.
3. Run \`check\` on the entry-point file after changing the import graph.
4. Build and run the smallest example before moving more code.
5. If the compiler reports a missing file or symbol, verify the spelling and the path relative to the importing file.

The compiler checks imported symbols and reports missing files or names; circular imports are rejected. Do not assume that an import has the behavior of a package manager or that it downloads dependencies.

## Keep dependencies understandable

Prefer a simple, visible module graph. Avoid splitting a tiny example into many files before the boundaries are useful. When a module depends on another module, make that relationship explicit in the source and keep the imported API narrow.

For reproducible builds, keep the files required by the program together in the project and document any external native dependencies separately. YLang's local import system is not a substitute for a dependency registry, version resolver, or system package manager.

## Troubleshooting

- **The imported file cannot be found:** check the exact relative path and file extension.
- **The imported function is unknown:** confirm the exported declaration name and the import spelling.
- **A cycle is reported:** restructure the modules so their dependency graph is not circular.
- **The project works in one directory but not another:** review relative paths and the source file from which they are resolved.
- **A module feature is rejected:** consult the [Module System](reference/MODULE-SYSTEM-SPEC.md) reference; do not assume aliases or qualified namespaces are supported.

For the exact grammar, visibility rules, and restrictions, read the [Module System](reference/MODULE-SYSTEM-SPEC.md) chapter.

## Practice

Create a file with a function that returns a fixed integer, import it from a separate \`.yl\` file, and call it from \`main\`. Check the entry point, build the program, and run it. Then rename the imported function deliberately and inspect the compiler's diagnostic before correcting it.
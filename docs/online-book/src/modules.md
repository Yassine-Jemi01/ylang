# Functions and Modules

Functions provide named units of behavior; imports let a source file use selected declarations from another `.yl` file.

## A local import

Given a sibling file named `math_tools.yl`:

```ylang
function answer() -> int {
    return 42;
}
```

A second file can import that function:

```ylang
from "./math_tools.yl" import answer;

function main() -> int {
    print(answer());
    return 0;
}
```

Relative paths are resolved from the importing source file. In this beta, imports are experimental and do not imply a central package registry or dependency solver. Import aliases and qualified module namespaces are not part of the current contract.

## Organize one module at a time

Keep each module focused, import only the names the file needs, and run `check` on the entry-point file after changing the import graph. The compiler checks imported symbols and reports missing files or names; circular imports are rejected.

For the exact grammar, visibility rules, and restrictions, read the [Module System](reference/MODULE-SYSTEM-SPEC.md) chapter.

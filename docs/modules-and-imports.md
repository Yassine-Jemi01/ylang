# Modules and imports (YLang 2.0 preview)

**Status:** experimental local-file import resolver. This is not yet a package manager or a complete module namespace system.

## Syntax

An import appears on its own top-level line, before or between declarations:

```ylang
from string_tools.yl import system_data()

function main() -> int {
    let string value = system_data();
    print(value);
    return 0;
}
```

The filename is resolved relative to the file containing the import. Quoted paths are also accepted, including paths with spaces. Multiple function names can be listed on one import line:

```ylang
from string_tools.yl import system_data, system_version()
```

The optional empty parentheses on imported names are accepted to match the original YLang prototype syntax. Imports currently name functions only.

## Resolution behavior

- Imported modules are read from local files; there is no registry, package manager, global search path, or network download.
- Nested imports are resolved recursively. A canonical file path is loaded only once in one build.
- Repeated imports of the same canonical file reuse the loaded module, so separate directives can import different functions without duplicating its definitions.
- Cyclic imports are rejected.
- Each listed function must be declared in the directly imported file.
- Diagnostics for declarations inside imported source use that source file's path and line number.
- The current resolver limits a project to 128 source files, 32 nested import levels, and 16 MiB of combined source.

## Important limitations

For now the compiler flattens loaded files into one program before semantic checking and code generation. This first implementation verifies the named imports exist, but **does not yet enforce module visibility**: other functions in included files share the same global compilation namespace. Duplicate function names are therefore still conflicts even when they come from different files.

There are no aliases, module-qualified names, public/private declarations, separate compiled module artifacts, incremental rebuild cache, package manifests, or versioned dependencies. Do not treat this prototype as a stable module ABI.

These limitations are deliberate work items for the v2 development line. The next module milestone is to add per-module symbol visibility and diagnostics, then decide how external libraries and package metadata should work.


## Cross-module function names

The current preview flattens imported source files into one compilation unit. Consequently, function names must be unique across all participating files; imported modules do not yet have independent namespaces. YLang reports this early as **E1103**, with locations for both definitions, rather than relying on a later C compiler redefinition error. Independent module namespaces and explicit aliases remain future language-design work.

# YLang 2.0 Module System — Proposal

**Status:** named local-file imports are implemented on the development branch; aliases and qualified module imports remain proposals. Treat this implementation as experimental until its CI matrix passes and the limitations below are resolved.

The compiler supports selective imports of top-level functions and globals using a local source path. It intentionally does not define a package registry or binary module ABI.

## 1. Current behavior and remaining limitations

A source file may import named declarations from another `.yl` file:

```yl
from "./string_tools.yl" import system_data;
```

The resolver loads transitive dependencies, resolves paths relative to the importing file, rejects cycles, and keeps each module's declarations and imports separate during semantic analysis. Only explicitly imported names are visible to the importer; helper declarations remain private to their source module. Generated C function names are module-qualified to avoid collisions. Imported modules may not define `main()`, and the root program must provide the entry point.

Current limitations: import aliases and `import "module.yl" as module` are not implemented; there is no package manifest or registry. Quoted paths and dotted filenames are supported, and `YLANG_PATH` searches colon-separated library directories after the importing file's directory. The NASM backend rejects multi-file imports until its module lowering is implemented and tested.

## 2. Proposed syntax

### Selective imports

Keep the existing spelling for compatibility:

```yl
from "math.yl" import clamp
from "text.yl" import trim, split
```

Optional empty parentheses after imported names may remain accepted during a deprecation period, but new examples should omit them because an import names a symbol, not a call.

### Aliases

Use an explicit `as` clause:

```yl
from "text.yl" import trim as trim_text

function main() -> int {
    let string cleaned = trim_text("  hello  ");
    print(cleaned);
    return 0;
}
```

An alias changes only the local name. It must not rename the original declaration in its defining module. Duplicate local names in the same scope must produce a compiler diagnostic even if they originate from different modules.

### Module-qualified imports

Allow importing a module under a local namespace name:

```yl
import "math.yl" as math

function main() -> int {
    print(math.clamp(12, 0, 10));
    return 0;
}
```

The exact punctuation can change during parser review, but the semantic rule should not: a qualified reference resolves only to an exported declaration in that specific module. It must not fall back to searching all flattened source files.

## 3. Name visibility rules

1. A source file is one module, identified internally by its canonical resolved path for local imports.
2. A module declaration belongs to that module only; it does not automatically become visible to the importing module.
3. A selective import creates a local binding only for each named symbol, or its explicit alias.
4. A qualified import creates one local module binding; declarations are accessed through that binding.
5. A symbol not imported or declared in the current module is an unknown-name error, even if another reachable module happens to define it.
6. A module may refer to its own declarations without importing itself.
7. Until an explicit visibility modifier exists, declarations intended for import are exported by default. A future `private` modifier may narrow visibility, but must not be silently inferred from filename or declaration order.
8. Duplicate local bindings are errors. Two modules may define the same function name without conflict when those functions are referenced through distinct module bindings.
9. Cyclic imports remain rejected in the first stable milestone. Supporting cycles later would require a separate declaration/signature collection pass and clear initialization rules.

## 4. Resolution and compilation architecture

Do not implement namespaces by rewriting every source file into a single string and then relying on the global symbol table.

Use a staged architecture:

1. **Resolve:** parse import directives and normalize paths relative to the importing file. Enforce current limits on module count, nesting, and total source size.
2. **Load once:** keep one module record per canonical path and report path errors against the import site.
3. **Declare:** collect top-level function and type signatures per module before analyzing bodies. This permits forward references within a module and gives a basis for future cycle support.
4. **Bind imports:** build a module-local symbol table mapping local names to a defining module and declaration.
5. **Analyze:** resolve every identifier using local declarations and explicit imports only. Report both the use site and, where useful, the conflicting binding.
6. **Lower/codegen:** pass a resolved symbol identity (module ID plus declaration ID) to the backend. The C backend may mangle internal names to avoid linker collisions; this is an implementation detail, not language-visible syntax.
7. **Diagnostics:** retain source spans for each original file so all errors point to the correct path and line.

The C backend can continue to compile the generated C as one translation unit initially, provided symbol identity and visibility have already been resolved correctly. Separate object files and a stable module ABI are later work, not prerequisites for correct source-level namespaces.

## 5. Diagnostics

Reserve stable diagnostics for module-specific failures. Proposed names are illustrative until added to the compiler's diagnostic registry:

- malformed import syntax or invalid alias;
- file not found, unreadable file, or unsupported path;
- imported symbol does not exist in the specified module;
- name is not visible in the current module;
- duplicate local binding caused by two imports or an import/local declaration collision;
- cyclic import, including the full cycle chain where practical.

Diagnostics should name the importing file and line. When a symbol exists but is not imported, suggest the explicit import only when the candidate is unambiguous.

## 6. Required acceptance tests

The module system should not be marked stable until all of these pass in the ordinary test suite:

- Existing single-file programs behave identically.
- Selective imports work with one and multiple symbols.
- Two import directives can import different symbols from the same file without duplicate definitions.
- Quoted paths with spaces and paths relative to the importer work.
- Nested dependencies work independent of declaration order.
- Cycles fail with a stable diagnostic.
- A function present in a reachable module but not explicitly imported cannot be called.
- An imported symbol can be renamed with `as`; the original name is not implicitly added to the local scope.
- Two modules can define functions with the same name and calls to each qualified name reach the correct implementation.
- Duplicate aliases and alias-vs-local-name collisions fail before backend compilation.
- A symbol imported from module A is not automatically re-exported through module B unless re-export syntax is explicitly designed.
- Compile-fail tests check diagnostic codes and source locations.
- GCC, Clang, Windows, and sanitizers continue to pass where supported.

## 7. Migration plan

1. Add resolver and semantic tests that describe the intended visibility rules.
2. Introduce per-module declaration tables while retaining the existing syntax.
3. Enforce selective-import visibility and reject accidental access to unimported declarations.
4. Add selective-import aliases.
5. Add qualified module imports and allow duplicate function names across modules.
6. Update the language guide and examples only after parser, semantic analysis, and all supported backends agree.

Each step should be a separate reviewable change. Do not add package manifests, registry downloads, inheritance, or raw pointers as part of this module-system work.

## 8. Non-goals

This proposal does not promise a package manager, dependency versioning, binary compatibility across compiler releases, dynamic linking, cyclic module initialization, or a Rust-equivalent safety guarantee. Local-file imports should remain deterministic and should not execute arbitrary build scripts.

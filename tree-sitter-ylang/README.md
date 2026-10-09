# tree-sitter-ylang

Tree-sitter grammar for **YLang 1.0**. This grammar covers the syntax documented in [the YLang language specification](../docs/language-spec.md), including declarations, functions, statements, expressions, comments, strings, and f-strings.

Tree-sitter provides an incremental concrete syntax tree for editor tooling. It does not replace YLang's compiler or semantic/type checker.

## Requirements

- Node.js and npm
- A C compiler (GCC or Clang)
- Tree-sitter CLI (installed by this package's npm dependency)

## Generate and test

From this directory:

```sh
npm install
npm run generate
npm test
npm run parse:examples
```

`npm run generate` creates the C parser and node type metadata under `src/`. The generated parser files are committed in this repository so editor integrations can compile them directly.

## Parse a YLang file

```sh
npx tree-sitter parse examples/hello.yl
```

## Grammar coverage

- Top-level globals and function definitions
- Typed function parameters and return types
- Mutable and `const` declarations
- Blocks, `if` / `else if` / `else`, `loop()`, `break`, `continue`, and `return`
- Assignments, function calls, `print(...)`, and expression statements
- Unary, arithmetic, comparison, equality, and boolean operators
- Integers, decimal floats, booleans, character literals, strings, escapes, and f-string interpolation
- Line comments beginning with `//`

Tree-sitter is a syntactic parser. Some restrictions remain the compiler's responsibility, such as type compatibility, variable initialization, const reassignment, supported `main` signature, and rejecting function calls inside f-string interpolation.

## Integration

The grammar is registered for the `.yl` file extension in `tree-sitter.json`. Its highlighting query is `queries/highlights.scm`.

For installation and automatic Tree-sitter highlighting in Neovim, see [`editors/neovim/README.md`](../editors/neovim/README.md) and run `bash editors/neovim/install.sh` from the repository root.

The first version is intentionally kept in the YLang repository under `tree-sitter-ylang/` so the grammar and language specification can evolve together. It can be moved to a dedicated `tree-sitter-ylang` repository later without changing the grammar name.

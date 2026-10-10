# YLang 2.0.0-dev

**A compiled programming language in active development, with explicit types, checked ownership, and actionable diagnostics.**

YLang is implemented in C. Its compiler tokenizes and parses `.yl` files, checks names, initialization and types, generates C, and invokes GCC or Clang to produce a native executable. It is a compiled language toolchain, not an interpreter.

This branch builds **YLang 2.0.0-dev**. It contains experimental language features beyond the stable 1.0.0 contract documented in `docs/language-spec.md`. It is not a final 2.0 release and does not claim Rust-level memory-safety guarantees.

## Quick start

### Requirements

- Linux (Fedora and Ubuntu) or Windows (MSYS2 UCRT64 / MinGW-w64; see [Windows setup](docs/windows.md))
- A C17 compiler to build YLang itself (`gcc` or `clang`)
- `make`
- GCC or Clang available on `PATH` to compile generated C into executables

On Fedora:

```sh
sudo dnf install gcc make clang
```

On Debian or Ubuntu:

```sh
sudo apt install build-essential clang make
```

### Build from source

```sh
git clone -b dev/lsp-foundation https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
make test
```

The compiler is written to `build/ylang`.

### Compile and run a program

Create `hello.yl`:

```ylang
function main() -> int {
    let const string name = "YLang";
    let int answer = 40 + 2;
    print(f"Hello from {name}!");
    print("Answer:", answer);
    return 0;
}
```

Then run:

```sh
./build/ylang check hello.yl
./build/ylang build hello.yl -o hello
./hello
```

To select Clang for the generated program:

```sh
./build/ylang build hello.yl -o hello --cc clang
```

## Command-line reference

```text
ylang --help
ylang --version
ylang check <file.yl>
ylang build <file.yl> [-o executable] [--cc gcc|clang]
ylang emit-c <file.yl> [-o generated.c]
ylang fix <file.yl> -o <fixed.yl>
```

- `check` parses and performs semantic/type checks without invoking a native compiler.
- `build` validates the source, generates C, and invokes GCC or Clang. Invalid YLang source does not produce an executable.
- `emit-c` writes the generated C source so it can be inspected.
- `fix` applies only the currently supported high-confidence fix (`pritn(...)` to `print(...)`) and requires `-o`. The output is written to the requested path; always run `check` on it. This is intentionally not a general-purpose automatic repair engine.

Errors include a stable diagnostic code, path, line/column, source excerpt, caret, and a hint when available. Exit status is nonzero when a command fails.

## Language overview

```ylang
let const int MAX = 100;
let int count = 0;

function add(int a, int b) -> int {
    return a + b;
}

function main() -> int {
    let string product = "YLang";
    print(f"Welcome to {product}");
    print("Sum:", add(10, 20));

    loop() {
        count = count + 1;
        print(f"Count: {count}");
        if (count >= 3) {
            break;
        }
    }

    if (count == 3 and MAX > 10) {
        print("Checks passed");
    }
    return 0;
}
```

See [`docs/language-spec.md`](docs/language-spec.md) for the complete supported syntax, type rules, runtime behavior, and explicit limitations. [`docs/architecture.md`](docs/architecture.md) explains the compiler pipeline.

## YLang 2.0 development

The development CLI now includes `ylang run <file.yl>`, which compiles source into a temporary directory, runs the resulting native program, removes its temporary files, and returns the program's exit status. Argument forwarding and debugger integration are not implemented yet.

YLang 2.0 is being developed separately from the stable 1.0.0 contract. The current work is exploratory and is not a complete 2.0 release. The development branch now has experimental scalar borrowing, string move/use-after-move checking with `clone`, `read_line()`, `len(string)`, and typed one-dimensional arrays (`T[]`) with checked indexing, `append`, `len`, and `clone`. The runtime still tracks heap allocations until process exit; scope-based deterministic cleanup, modules, a full standard library, and the dedicated native backend remain unfinished. See the [2.0 design](docs/v2-design.md), [development roadmap](docs/roadmap.md), [ownership model](docs/ownership-and-borrowing.md), and [standard-library plan](docs/standard-library.md).

### Experimental 2.0 array example

```ylang
function main() -> int {
    let int[] scores = [10, 20, 30];
    scores[1] = 42;
    append(scores, 99);
    print(scores[1], len(scores)); // 42 4
    let int total = 0;
    for (int score in scores) {
        total = total + score;
    }
    print("Total:", total); // 181
    let int[] backup = clone(scores);
    backup[0] = 7;
    print(scores[0], backup[0]); // 10 7
    return 0;
}
```

This syntax is available on `dev/lsp-foundation`, not part of the stable 1.0.0 language contract. Empty array literals need an explicit contextual type (for example `let int[] values = []`); nested arrays are not supported, and arrays print as `[array len=N]`.

## What is included in 1.0.0

- Explicit declarations (`let type name`) and constants (`let const type name`)
- `int` (signed 64-bit), `float` (64-bit), `bool`, single-byte `char`, `string`, and `void` return types
- Functions and return statements
- `if` / `else if` / `else`, `loop()`, `break`, and `continue`
- Arithmetic, comparisons, boolean operators, function calls, and assignments
- `print(...)` and f-string interpolation for supported expressions
- Name/type checks, uninitialized-read checks, constant-assignment checks, and source-located diagnostic messages
- Runtime checks for integer overflow and division by zero
- C code generation and native compilation using GCC or Clang
- A deliberately narrow safe-fix command

## Explicit non-goals for this release

YLang 1.0.0 does **not** implement arrays, raw pointers/references, classes/OOP, `try`/`catch`, `for`/`while`, modules, generics, or a dedicated LLVM/native-code backend. These are not silently approximated; programs using unsupported syntax are rejected. A final ownership/borrowing or garbage-collection model is not defined. `char` is one byte, not a Unicode scalar value.

The C backend uses generated runtime helpers and process-lifetime storage for some formatted strings. Long-running programs that repeatedly create f-string values may grow in memory usage. Do not use this release for security-critical code or to process hostile source as a hardened sandbox. Generated programs are ordinary native programs with the permissions of the user who runs them.

## Build and test

```sh
make clean
make
make test
```

Optional checks if tools are available:

```sh
make CC=clang test
make sanitize
```

The tests exercise successful compilation/output, GCC/Clang parity when Clang is installed, diagnostics, rejection of invalid programs, safe fixes, integer overflow, and division by zero.

## Install (optional)

```sh
sudo make install
```

The default installation path is `/usr/local/bin/ylang`. Use `sudo make uninstall` to remove it, or customize the prefix:

```sh
make install PREFIX="$HOME/.local"
```

## Repository layout

- `src/` — compiler implementation (lexer, parser, semantic analysis, code generator, CLI)
- `include/ylang/` — public-facing header declarations
- `examples/` — sample YLang programs
- `tests/` — compiler and regression tests
- `docs/` — language specification and contributor notes

## Contributing

Bug reports and focused improvements are welcome. Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before opening a pull request. Changes to supported syntax or semantics must update the specification and tests in the same change.

## License

YLang is distributed under the MIT License. See [`LICENSE`](LICENSE).

## Project resources

![YLang logo](assets/branding/ylang-logo-full.svg)

- [YLang Book (PDF)](docs/book/YLang-Book.pdf)
- [Language specification](docs/language-spec.md)
- [Architecture](docs/architecture.md)
- [Windows setup guide](docs/windows.md)
- [Tree-sitter grammar and parsing tests](tree-sitter-ylang/README.md)
- [Neovim Tree-sitter integration](editors/neovim/README.md)
- [VS Code language support, compiler diagnostics, and Code Runner setup](editors/vscode/README.md)

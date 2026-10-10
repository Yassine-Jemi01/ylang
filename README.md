# YLang 1.0.0

**A small compiled programming language with explicit types and actionable diagnostics.**

YLang is implemented in C17. Its compiler tokenizes and parses `.yl` files, checks names, initialization and types, generates C, and invokes GCC or Clang to produce a native executable. The compiler currently targets POSIX systems; Linux and macOS are the supported portability targets. Native Windows support remains future work. It is a compiled language toolchain, not an interpreter.

YLang **1.0.0 is the first stable release of the language subset documented in the specification**. The syntax and behavior listed as supported below are the v1.0 contract. This release is deliberately small; it does not claim to implement every feature planned for YLang, and it does not claim Rust-level memory safety.

## Quick start

### Requirements

- Linux (Fedora and Ubuntu) or macOS 13+ (CI-tested targets; Windows is not supported yet)
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

On macOS, install the Xcode Command Line Tools and Homebrew toolchain:

```sh
xcode-select --install
brew install make llvm
# Use gmake instead of the built-in BSD make for every build/test command
```

For the optional GUI runtime, also install SDL2 and pkg-config:

```sh
brew install sdl2 pkg-config
```

### Build from source

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
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

## Optional GUI runtime (SDL2)

YLang now includes an optional SDL2-backed C GUI runtime for creating a window, processing close events, clearing/presenting frames, and drawing filled rectangles. SDL2 is cross-platform, but the current YLang compiler driver is still POSIX-based and native Windows builds are not supported yet. The GUI runtime is a C API today; direct calls from `.yl` code await a defined foreign-function interface.

Install SDL2 development files and `pkg-config`, then build the runtime and example:

Linux (Fedora): `sudo dnf install SDL2-devel pkgconf-pkg-config`

Linux (Debian/Ubuntu): `sudo apt install libsdl2-dev pkg-config`

macOS: `brew install sdl2 pkg-config`

```sh
gmake gui gui-example
./build/gui-example
```

The example opens a window, paints the background, and draws a rectangle. On headless systems, compilation works but running it requires a display session. To install the optional static library and header, use `sudo gmake install-gui` on macOS (or `sudo make install-gui` on Linux).

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
- [VS Code language support, compiler diagnostics, and Code Runner setup](editors/vscode/README.md)
- [GUI runtime API](include/ylang/gui.h) and [example](examples/gui.c)

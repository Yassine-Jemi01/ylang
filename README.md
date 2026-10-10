# YLang 1.1.0

**A small compiled programming language with explicit types and actionable diagnostics.**

YLang is implemented in C17. Its compiler tokenizes and parses `.yl` files, checks names, initialization and types, generates C, and invokes GCC or Clang to produce a native executable. The compiler currently targets POSIX systems; Linux and macOS are the supported portability targets. Native Windows support remains future work. It is a compiled language toolchain, not an interpreter.

YLang **1.1.0** extends the first stable language subset with explicit conversions and checked integer parsing. It remains deliberately scoped and does not claim to implement every planned feature or Rust-level memory safety.

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

## Standard math and arrays

YLang includes a checked `math` namespace for floating-point functions:

```ylang
let float root = math.sqrt(81.0);
let float power = math.pow(2.0, 8.0);
```

Fixed-size arrays use an inferred length, homogeneous initializers, and bounds-checked indexing. Functions can accept arrays with `int values[]`; `length(values)` returns the array length, including for parameters:

```ylang
let int scores[] = [10, 20, 30];

function main() -> int {
    scores[1] = 42;
    print(scores[0], scores[1], scores[2]);
    return 0;
}
```

Explicit conversions include `to_float(int)`, range-checked `to_int(float)`, and `to_string(value)` for int/float/bool/char/string. Use `string.is_int(text)` before `string.parse_int(text)` for untrusted input; invalid or out-of-range parsing raises a runtime error. `io.file_exists(path)` lets programs check a path before reading. String helpers and I/O are also available: `string.length`, `string.contains`, `string.starts_with`, `string.ends_with`, `string.concat`, `string.replace`, `io.read_line()`, `io.read_file(path)`, and `io.write_file(path, content)`. The new `path` namespace provides `path.exists(path)`, `path.basename(path)`, and `path.extension(path)`. On Linux/macOS, `image.open(path)` safely launches the system default viewer (using an argument vector rather than a shell command), returning whether the viewer launcher succeeded; it does not decode/render images inside YLang yet. File I/O is synchronous and text-only; read errors terminate with a runtime error, while writes return a boolean status. Run `make test` to test these APIs alongside math, arrays, and bounds protection. Arrays can be passed to functions with their length supplied automatically, but cannot yet be returned or nested; object-oriented classes are not implemented yet.


## What is included in 1.0.0

- Explicit declarations (`let type name`) and constants (`let const type name`)
- `int` (signed 64-bit), `float` (64-bit), `bool`, single-byte `char`, `string`, and `void` return types
- Functions and return statements
- `if` / `else if` / `else`, `loop()`, `while`, C-style `for`, `break`, and `continue`
- Arithmetic, comparisons, boolean operators, function calls, and assignments
- `print(...)`, f-string interpolation, string utilities, path inspection, OS image-viewer launching, standard-input line reading, and text file I/O
- Name/type checks, uninitialized-read checks, constant-assignment checks, and source-located diagnostic messages
- Runtime checks for integer overflow and division by zero
- C code generation and native compilation using GCC or Clang
- A deliberately narrow safe-fix command

## Explicit non-goals for this release

YLang's current language subset does **not** implement classes/OOP, raw pointers/references, `try`/`catch`, modules, generics, or a dedicated LLVM/native-code backend. Arrays are fixed-size and cannot yet be passed to or returned from functions; nested and const arrays are not supported. These are not silently approximated; programs using unsupported syntax are rejected. A final ownership/borrowing or garbage-collection model is not defined. `char` is one byte, not a Unicode scalar value.

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

The tests exercise successful compilation/output, GCC/Clang parity when Clang is installed, diagnostics, rejection of invalid programs, safe fixes, integer overflow, division by zero, math and string built-ins, line input, text file I/O, array mutation and bounds checks, `loop()`, `while`, C-style `for`, and native optimization-level selection.

### Native optimization

YLang emits C and compiles it with GCC or Clang. Native builds default to `-O2`; set `YLANG_OPT_LEVEL` to `0`, `1`, `2`, `3`, or `s` to select the compiler optimization level:

```sh
YLANG_OPT_LEVEL=3 ylang build app.yl -o app
```

Use `0` for debugging, `2` for the default balance, `3` when testing maximum speed, and `s` to favor smaller binaries. Actual speed depends on the program, compiler, and hardware; YLang does not claim to outperform every language without reproducible benchmarks.

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

# YLang 2.0.0

**A small compiled programming language with explicit types and actionable diagnostics.**

YLang is implemented in C17. Its compiler tokenizes and parses `.yl` files, checks names, initialization and types, generates C, and invokes GCC or Clang to produce a native executable. Linux and macOS are the primary tested targets; experimental native Windows support is available for 64-bit Windows with MinGW-w64. Windows support currently has a focused CI smoke suite and is not yet a claim of complete platform parity. It is a compiled language toolchain, not an interpreter.

YLang **2.0.0** is the stable release of the 2.0 line. The stable compatibility commitment covers the documented core language and the default C-backend workflow. Newer subsystems—named imports, typed enums and structs, SDL2-backed GUI/Game2D APIs, the NASM backend, native Windows support, and optional YSPM packaging—remain explicitly experimental and may change independently of the stable core. Linux and macOS are the primary tested targets; native Windows x86-64 uses a limited MinGW-w64 smoke-tested path. YLang does not claim Rust-level memory safety or provide a security sandbox.

## The online book

The [YLang Book](https://yassine-jemi01.github.io/ylang/learn/index.html) is a searchable, multi-page HTML guide with a chapter sidebar, previous/next navigation, theme switching, and copyable examples. It combines step-by-step tutorials with the canonical language, platform, compiler, and library references; it is rendered as a website, not served as raw Markdown.

## Quick start

### Requirements

- Linux (Fedora and Ubuntu), macOS 13+, or experimental Windows x86-64 with MinGW-w64 (see [Windows setup](docs/WINDOWS.md))
- A C17 compiler to build YLang itself (`gcc` or `clang`)
- GNU Make (`make` on Linux; `gmake` on macOS)
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

### Experimental Windows setup

Native Windows builds are tested with MinGW-w64 in an MSYS2 MINGW64 shell. MSVC is not supported by this initial port.

See [the Windows setup guide](docs/WINDOWS.md) for MSYS2 installation, SDL2 dependencies, native compile/run instructions, and the current limitations.

### Build from source

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
make test
```

On macOS, use `gmake` instead of `make` for these commands (Homebrew's GNU Make; Apple's built-in `make` is BSD Make). The compiler is written to `build/ylang`.

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


### Experimental NASM backend (Linux x86-64)

The normal `ylang build` command still uses the C backend. An experimental
backend can emit NASM assembly and build a restricted subset directly with
NASM and `ld`, without GCC/Clang participating in compilation of that YLang
program. The YLang compiler itself is still written in C and must be built
using the normal C toolchain.

On Fedora, install the assembler and linker tools:

```sh
sudo dnf install nasm binutils
```

Emit assembly for inspection:

```sh
./build/ylang emit-asm examples/your-program.yl -o build/your-program.asm
```

Or assemble and link in one step:

```sh
./build/ylang build-asm examples/your-program.yl -o build/your-program
./build/your-program
```

To measure a small arithmetic workload's end-to-end process latency, build the
benchmark and run it repeatedly with output redirected so terminal rendering
does not dominate the measurement:

```sh
./build/ylang build-asm benchmarks/nasm-speed.yl -o build/nasm-speed
python3 benchmarks/bench-nasm.py build/nasm-speed --runs 1000 --target-ms 1
```

The benchmark reports minimum, median, mean, and 95th-percentile latency.
Its timing includes process launch and exit, so a median below 1 ms is a
machine-dependent target rather than a guaranteed performance claim.

This backend is currently limited to Linux x86-64 scalar programs using
`int`, `bool`, `char`, and `string`, local variables, user-defined
functions with up to six scalar parameters, integer expressions, branches,
loops, returns, and `print`. Global declarations, arrays, floats, f-strings,
string comparisons, short-circuit `and`/`or`, and standard-library calls
are not supported yet. The established C backend remains the default. A backend-neutral IR and general-purpose native backend are future work; see the [backend roadmap](docs/BACKEND-ROADMAP.md).

## Structs (experimental)

This experimental feature set includes typed struct declarations, positional construction, field reads, struct values passed to or returned from functions, and type-checked mutable field assignment in the C backend. NASM deliberately rejects struct values for now. See [struct reference](docs/STRUCTS.md) and [runnable example](examples/structs.yl).

## Typed enums (experimental)

Typed unit enums and exhaustive `match` statements are available as an experimental 2.0 extension; they are not part of the stable 2.0.0 core compatibility guarantee.

```yl
enum Direction { North, East, South, West }

function main() -> int {
    let Direction facing = Direction.North;
    match (facing) {
        Direction.North -> { print("north"); }
        Direction.East -> { print("east"); }
        Direction.South -> { print("south"); }
        Direction.West -> { print("west"); }
    }
    return 0;
}
```

See [enum reference](docs/ENUMS.md) and [runnable example](examples/enums.yl). Payload variants, generic enums, and matching bindings are not implemented in this milestone.

## Multi-file imports (experimental)

YLang can import named functions and global variables from another source file. Paths are resolved relative to the importing file first.

```yl
from "./math_tools.yl" import double_value;

function main() -> int {
    print(double_value(21));
    return 0;
}
```

For reusable local libraries, set `YLANG_PATH` to a colon-separated list of directories (Linux/macOS):

```sh
YLANG_PATH="$HOME/ylang-libs" ylang build main.yl -o main
```

Imports can be nested, and repeated references to the same canonical file reuse the loaded module. Circular imports, missing files/names, duplicate imports, and imports of `main()` are rejected with diagnostics. The C backend supports the module graph; the experimental NASM backend supports imports only when all imported features are in its supported subset. Import aliases, qualified module namespaces, package manifests, and a package registry are not implemented yet.

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

YLang includes an optional SDL2-backed GUI namespace for `.yl` programs: `gui.init()`, `gui.create_window(...)`, `gui.poll()`, `gui.clear(...)`, `gui.draw_rect(...)`, `gui.present()`, `gui.last_error()`, `gui.close()`, and `gui.shutdown()`. The compiler checks argument types without requiring SDL2; native builds that use `gui.*` need SDL2 development files and `pkg-config`. Linux and macOS remain the primary tested targets; native Windows x86-64 with MinGW-w64 is experimental. See the [Windows setup guide](docs/WINDOWS.md) for its tested scope and limitations.

Install SDL2 development files and `pkg-config`, then build a YLang GUI program:

Linux (Fedora): `sudo dnf install SDL2-devel pkgconf-pkg-config`

Linux (Debian/Ubuntu): `sudo apt install libsdl2-dev pkg-config`

macOS: `brew install sdl2 pkg-config`

```sh
# Linux
./build/ylang build examples/gui.yl -o build/gui-example

# macOS
./build/ylang build examples/gui.yl -o build/gui-example

./build/gui-example
```

The example opens a window, paints the background, and draws a rectangle, then closes cleanly. It is a minimal smoke example, not a complete game loop. On headless systems, compilation works but running it requires a display session. To install the optional runtime files for builds outside the source tree, use `sudo gmake install-gui` on macOS (or `sudo make install-gui` on Linux).

Errors include a stable diagnostic code, path, line/column, source excerpt, caret, and a hint when available. Exit status is nonzero when a command fails.

## Optional Linux system package (YSPM)

[YSPM](https://github.com/Yassine-Jemi01/yspm) is your Linux system package manager and uses the native `.yspkg` format. YLang can be packaged for a compatible YSPM system; this is separate from compiling YLang projects or resolving `.yl` imports.

Build a native package locally (Linux x86-64, Go, GCC, GNU Make, and a YSPM checkout with .yspkg build support required):

    cd ..
    git clone https://github.com/Yassine-Jemi01/yspm.git
    cd yspm
    go build -o yspm ./cmd/yspm
    cd ../ylang
    YSPM_BIN="../yspm/yspm" bash packaging/yspm/build-package.sh

This creates `build/ylang-yspm/ylang-<version>-x86_64.yspkg`. The package includes the compiler, .yl standard library, GUI runtime source/header, license, and available documentation. The stable release workflow builds and validates a Linux x86-64 package from the exact tested commit before attaching it to the release. Prerelease assets remain separate from the stable release.

When a release includes a .yspkg asset, download it and install it with:

    sudo yspm install ./ylang-<version>-x86_64.yspkg

YSPM manages Linux system packages, not project-local YLang module dependencies. The compiler still needs GCC or Clang on the host to compile generated C, and GUI/game builds need SDL2 development files plus pkg-config. See [the YSPM guide](docs/YSPM.md) for the package contents and limits.

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


## Experimental owned strings

The compiler branch contains a separate `owned string` type with explicit cloning (`string.own(value)`), runtime move transfer (`move(name)`), deterministic cleanup, and local `&string` / `&mut string` borrow checks. This is experimental and **does not** make legacy `string` values move-only or provide a complete Rust-like borrow checker. Globals, struct fields, borrowed/owned function parameters, and borrowed/owned return values are rejected until their lifetime rules are implemented.

See the [owned-string guide](docs/OWNED-STRINGS.md) and [memory-model RFC](docs/MEMORY-MODEL.md).

## Supported language features

- Explicit declarations (`let type name`) and constants (`let const type name`)
- `int` (signed 64-bit), `float` (64-bit), `bool`, single-byte `char`, `string`, and `void` return types
- Functions and return statements
- `if` / `else if` / `else`, `loop()`, `while`, C-style `for`, `break`, and `continue`
- Integer literals support decimal and hexadecimal notation (for example, `0xFF`). Arithmetic, comparisons (including lexicographic string ordering), boolean operators, function calls, assignments (`=` and `+=`), and postfix numeric increment (`++`) are supported.
- `print(...)`, f-string interpolation, string utilities, path inspection, OS image-viewer launching, standard-input line reading, and text file I/O
- Name/type checks, uninitialized-read checks, constant-assignment checks, and source-located diagnostic messages
- Runtime checks for integer overflow and division by zero
- C code generation and native compilation using GCC or Clang
- A deliberately narrow safe-fix command

## Explicit limitations of the stable core

The stable core does **not** implement classes/OOP, raw pointers/references, `try`/`catch`, generics, or a dedicated LLVM/native-code backend. Named file imports are available as an experimental extension, not as a stable package/module ecosystem. Arrays are fixed-size: functions may accept array parameters (with the length passed automatically), but arrays cannot be returned or nested, and `const` arrays are not supported. These are not silently approximated; programs using unsupported syntax are rejected. A final ownership/borrowing or garbage-collection model is not defined. `char` is one byte, not a Unicode scalar value.

The C backend uses generated runtime helpers and process-lifetime storage for some formatted strings. Long-running programs that repeatedly create f-string values may grow in memory usage. Do not use this release for security-critical code or to process hostile source as a hardened sandbox. Generated programs are ordinary native programs with the permissions of the user who runs them.

## Build and test

On Linux:

```sh
make clean
make
make test
```

On macOS, use GNU Make (`gmake`):

```sh
gmake clean
gmake
gmake test
```

Optional checks if tools are available (replace `make` with `gmake` on macOS):

```sh
make CC=clang test
make sanitize
```

The tests exercise successful compilation/output, GCC/Clang parity when Clang is installed, diagnostics, rejection of invalid programs, safe fixes, integer overflow, division by zero, string comparisons and utilities, line input, text file I/O, array mutation and bounds checks, `loop()`, `while`, C-style `for`, and native optimization-level selection.

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

- [YLang Book (online HTML)](https://yassine-jemi01.github.io/ylang/learn/index.html)
- [YLang Book (PDF)](docs/book/YLang-Book.pdf)
- [Language specification](docs/language-spec.md)
- [Architecture](docs/architecture.md)
- [YLang 2.0 design roadmap](docs/YLANG-2.0-DESIGN.md)
- [YLang 2.0.0 release notes and compatibility policy](docs/CHANGELOG-2.0.0.md)
- [VS Code YLang 2.0 syntax highlighting, compiler diagnostics, and Code Runner setup](editors/vscode/README.md)
- [Neovim YLang 2.0 syntax highlighting and native diagnostics integration](editors/nvim/README.md)
- [GUI runtime API](include/ylang/gui.h) and [example](examples/gui.c)
- [Native Windows setup and limitations](docs/WINDOWS.md)
- [YSPM Linux system-package integration](docs/YSPM.md)
- [Game2D guide](docs/GAME2D.md), [playable Dodge example](examples/games/dodge.yl), and [helper module](stdlib/game2d.yl) (build with `make game2d-example`)


## Compiler developer checks

`make test` runs the regression suite plus standalone lexer/parser unit tests. The SDL2 runtime smoke test is optional: when SDL2 development files or `pkg-config` are unavailable, the suite prints an explicit `SKIP` message instead of failing unrelated tests.

On Linux with Clang and libFuzzer available, run a bounded lexer/parser fuzz smoke test with:

```sh
make fuzz-smoke FUZZ_CC=clang
```

The C runtime emitted into generated programs is sourced from [runtime/runtime_template.c](runtime/runtime_template.c) and embedded as a byte array to avoid overlength C string literals. After editing the template, regenerate and verify it with `make regen-runtime-template` and `make check-runtime-template`.

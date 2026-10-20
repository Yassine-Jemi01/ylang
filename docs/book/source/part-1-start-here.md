# Part I — Start Here

# Chapter 1. What is YLang?

YLang is a small compiled programming language. You write source code in text files ending with `.yl`, then use the YLang compiler to check your program, generate C, and invoke GCC or Clang to produce a native executable. It is not an interpreter: your `.yl` source is compiled before the resulting program runs.

The normal workflow has three stages:

1. **Check the source.** YLang parses the code and checks names, initialization, and types.
2. **Generate C.** The compiler translates the checked program into C that can be inspected.
3. **Build the executable.** GCC or Clang compiles that C code into a native program.

The compiler itself is implemented in C17. That implementation detail does not mean you write C when using YLang. Your source remains YLang, and the C backend is the default translation path.

## What is stable in 2.0.1?

The stable core includes explicit declarations, constants, scalar types, functions, control flow, fixed-size homogeneous arrays, strings, formatted strings, standard input, text-file operations, path utilities, math helpers, and the documented command-line tools. Explicit typing means the compiler does not silently change an integer into a float to make an expression compile.

YLang diagnostics include a code, file location, line and column, a source excerpt, and a hint when available. These details are designed to make errors easier to investigate.

## What is experimental?

Several features developed during the 2.0 cycle are available to evaluate but have separate compatibility guarantees: named imports, structs, enums, the optional SDL2 GUI and Game2D APIs, the NASM backend, native Windows support, and the optional YSPM package. This book marks those features clearly. Do not assume experimental syntax or APIs cannot change.

The stable language is suitable for learning programming, command-line tools, calculations, and text-processing programs. The current Game2D layer is a small learning aid, not a complete game engine. YLang does not claim Rust-level memory safety or provide a security sandbox.

---

# Chapter 2. Installing YLang

The simplest way to start is to build YLang from its source repository. This gives you the compiler, tests, examples, and documentation in one place.

## 2.1 Requirements

For the normal C backend, use Linux or macOS with a C17 compiler and GNU Make. Git is useful for cloning the source. Ordinary command-line programs do not require SDL2; GUI and Game2D builds additionally need SDL2 development files and `pkg-config`.

Native Windows support is experimental for 64-bit Windows with MinGW-w64 inside an MSYS2 MINGW64 shell. It is not full platform parity, and MSVC is not supported by this release.

## 2.2 Install the tools

On Fedora:

```sh
sudo dnf install gcc make clang git
```

For optional GUI and Game2D features:

```sh
sudo dnf install SDL2-devel pkgconf-pkg-config
```

On Debian or Ubuntu:

```sh
sudo apt install build-essential clang make git
```

For optional GUI and Game2D features:

```sh
sudo apt install libsdl2-dev pkg-config
```

On macOS, install the Xcode Command Line Tools and GNU Make:

```sh
xcode-select --install
brew install make llvm git
```

Use `gmake` instead of the system `make` for build and test commands on macOS. To build optional GUI examples, also install `sdl2` and `pkg-config` with Homebrew.

## 2.3 Get the source and build

Clone the official repository and enter its folder:

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
make test
./build/ylang --version
```

On macOS, use `gmake` in place of `make`. The compiler is written to `build/ylang`. Running the test suite is recommended before learning the language because it verifies that your local toolchain is usable.

If a command fails, inspect the first error before trying random fixes. A missing compiler or dependency can produce multiple follow-up errors.

## 2.4 Install it (optional)

You can run the compiler from the checkout with `./build/ylang`. To install it system-wide, use the repository's install target:

```sh
sudo make install
```

A user-local install can avoid modifying system directories:

```sh
make install PREFIX="$HOME/.local"
```

Ensure `$HOME/.local/bin` is on your `PATH` if you want to invoke `ylang` from any folder. Check the current README for the exact installation target and prefix supported by your version.

## 2.5 Verify the version

```sh
ylang --version
```

The compiler should report version `2.0.1`. If you see an older version, your shell may be finding another executable earlier in `PATH`. Compare `command -v ylang` with `./build/ylang --version` to diagnose which one you are running.

---

# Chapter 3. Your First Program

Create a file named `hello.yl`:

```ylang
function main() -> int {
    print("Hello, YLang!");
    return 0;
}
```

Build and run it:

```sh
ylang check hello.yl
ylang build hello.yl -o hello
./hello
```

Expected output:

```text
Hello, YLang!
```

If YLang is not installed, replace `ylang` with `./build/ylang` in the first two commands.

## Read every line

`function main() -> int` declares the program entry point. The keyword `function` begins a function, `main` is its name, and `-> int` says the function returns an integer.

The brace `{` begins the function body; `}` ends it. YLang uses braces to group statements into blocks. The semicolon marks the end of each ordinary statement.

`print("Hello, YLang!");` writes the text followed by a newline. `return 0;` ends the function and returns zero, conventionally signalling successful completion to the operating system.

## Why check first?

`ylang check hello.yl` parses the code and performs semantic checks without invoking the native C compiler. It is a fast way to get feedback while editing.

`ylang build hello.yl -o hello` validates the source and then creates an executable at the path provided after `-o`. Every time you change the source, rebuild before running it; otherwise, you may still be launching the old executable.

Try changing the greeting, running `check`, rebuilding, and launching the executable again. This simple loop—edit, check, build, run—is the most useful habit to develop at the beginning.

---

# Chapter 4. The Compiler Commands

YLang keeps its command-line interface small. Each command is intended for a specific task.

## Check source code

```sh
ylang check hello.yl
```

This parses and checks the source without building an executable. A successful check confirms that the compiler accepts the code at the checking stage; it does not guarantee that opening a file or performing another runtime operation will succeed.

## Build a program

```sh
ylang build hello.yl -o hello
ylang build hello.yl -o hello --cc clang
```

The first command builds using the default compiler selection. The second explicitly selects Clang for compiling the generated C. GCC or Clang must be available on `PATH`.

## Inspect the generated C

```sh
ylang emit-c hello.yl -o build/hello.c
```

The output is useful for debugging and understanding the translation pipeline. Prefer writing and maintaining your source in YLang rather than depending on undocumented implementation details of generated C.

## Get help and check the version

```sh
ylang --help
ylang --version
```

Use these when a tutorial mentions an unfamiliar command or when you suspect that several installations exist on your system.

## The limited fix command

```sh
ylang fix broken.yl -o fixed.yl
ylang check fixed.yl
```

The current `fix` command only performs a high-confidence repair for the misspelling `pritn(...)` to `print(...)`. It is not a general-purpose code repair engine. Review its output and run `check` before building it; fixing spelling cannot infer what an algorithm should do.

Keep the distinction clear: `check` validates source, `build` creates an executable, `emit-c` writes generated C, and `fix` applies one narrow supported correction.

# Part V — Extensions and Reference

# Chapter 21. GUI and Game2D (Experimental)

YLang's optional GUI runtime is built on SDL2. The normal command-line workflow does not need SDL2, but builds that use GUI calls require SDL2 development files and `pkg-config`. Running an interactive window also requires a graphical session.

## The GUI namespace

The current milestone supports one active window per process. The API includes:

- `gui.init()` to initialize the runtime.
- `gui.create_window(title, width, height)` to create a window.
- `gui.poll()` to process window events.
- `gui.clear(red, green, blue)` to clear the frame.
- `gui.draw_rect(x, y, width, height, red, green, blue)` to draw a filled rectangle.
- `gui.present()` to display a frame.
- `gui.last_error()` to read the latest runtime error.
- `gui.close()` and `gui.shutdown()` to release resources.

A basic loop should check failure results and release resources before leaving the program:

```ylang
function main() -> int {
    if (not gui.init()) {
        print(gui.last_error());
        return 1;
    }

    if (not gui.create_window("YLang GUI", 800, 500)) {
        print(gui.last_error());
        gui.shutdown();
        return 1;
    }

    loop() {
        if (not gui.poll()) { break; }
        if (not gui.clear(24, 28, 38)) { break; }
        if (not gui.draw_rect(80, 80, 240, 120, 22, 160, 180)) { break; }
        if (not gui.present()) { break; }
    }

    gui.close();
    gui.shutdown();
    return 0;
}
```

RGB channels must lie between zero and 255, and rectangle dimensions must be positive. A `false` return may mean a runtime failure or a request to stop, depending on the operation. Check `gui.last_error()` for the latest explanatory message.

## The Game2D helper

The experimental `stdlib/game2d.yl` module is a small source-level helper layer over the SDL2 runtime. It demonstrates event polling, a frame loop, keyboard and mouse state, elapsed-time measurement, rectangle drawing, collision checks, and a playable Dodge example.

This is not yet a full game engine. The current helper has no general sprite or image-decoding pipeline, text renderer, audio system, animation framework, physics engine, scene manager, or multiple active windows. The score in the example is printed to the terminal instead of drawn inside the window.

Read `docs/GAME2D.md` for the current API, required packages, example build command, and controls. Compilation in a headless environment does not prove interactive rendering works; a window needs a graphical session.

---

# Chapter 22. The NASM Backend (Experimental)

The usual `ylang build` command uses the C backend. The experimental NASM backend targets only a limited subset of programs on Linux x86-64; it is not a replacement for the default backend.

On Fedora, install the assembler and linker utilities:

```sh
sudo dnf install nasm binutils
```

To emit an assembly file for inspection:

```sh
ylang emit-asm examples/your-program.yl -o build/your-program.asm
```

To assemble and link a supported source file:

```sh
ylang build-asm examples/your-program.yl -o build/your-program
./build/your-program
```

The tested subset includes scalar values such as `int`, `bool`, `char`, and `string`, local variables, user-defined functions with a limited number of scalar parameters, integer expressions, branches, loops, returns, and `print`.

Features such as global declarations, arrays, floats, f-strings, string comparisons, short-circuit boolean operations, and standard-library calls are outside the current subset. An assembly command failing on a valid YLang program does not automatically mean the program is invalid in the stable C backend.

Use NASM when you are specifically evaluating the experimental backend. Use the C backend for ordinary programs.

---

# Chapter 23. Platform Support and YSPM Packages

## Linux and macOS

Linux and macOS are the primary tested targets for the default stable C-backend workflow. Build the compiler with GCC or Clang, then use `ylang build` to compile your programs. GUI/Game2D development dependencies are optional.

## Windows (Experimental)

Native Windows support targets 64-bit Windows with MinGW-w64 in an MSYS2 MINGW64 shell. This port has focused smoke tests; it does not promise full feature parity. MSVC is not supported. Follow `docs/WINDOWS.md` for the current installation steps and limits.

## The optional YSPM package

YSPM is a Linux system package manager. YLang provides an optional `.yspkg` package for compatible YSPM systems on Linux x86-64. This package installs the compiler and related library files; it does not manage dependencies for source-level imports.

Download `ylang-2.0.1-x86_64.yspkg` from the official release and install it on a compatible system:

```sh
sudo yspm install ./ylang-2.0.1-x86_64.yspkg
```

The package does not automatically install GCC/Clang, GNU Make, SDL2 development files, or `pkg-config`. The host still needs GCC or Clang to compile generated C, and GUI builds need SDL2 development dependencies. The package is not a Windows or macOS installer.

---

# Chapter 24. What YLang 2.0.1 Does Not Have

Being clear about the limitations prevents a tutorial from promising features that do not exist.

The stable core does not include implicit numeric conversions, classes, object-oriented inheritance, methods on structs, generics, implemented `Option<T>` and `Result<T, E>` types, general exception handling, or a complete LLVM/independent native backend.

Arrays cannot be returned from functions or nested inside other arrays. Constant arrays and whole-array assignment are not supported. The `char` type is one byte, and string length and case conversion are byte-oriented rather than Unicode-aware.

The experimental extension systems have additional limits. Imports do not provide a package registry, manifest, dependency solver, alias system, or qualified module namespaces. Structs and enums lack advanced forms such as methods, inheritance, payload variants, and generic variants. GUI/Game2D is a small optional layer, not a complete engine. NASM only targets its documented Linux x86-64 subset. Windows and YSPM remain experimental or platform-specific.

YLang does not claim Rust-level memory safety and does not provide a security sandbox. Do not assume that a successful compile makes the program safe for hostile inputs.

---

# Chapter 25. Quick Reference

## Declarations and functions

```ylang
let int count = 0;
let string name = "YLang";
let const float pi_approx = 3.14;

function add(int a, int b) -> int {
    return a + b;
}

function main() -> int {
    print(add(20, 22));
    return 0;
}
```

## Conditions and loops

```ylang
if (count >= 10) {
    print("Ten or more");
} else {
    print("Less than ten");
}

while (count < 3) {
    count += 1;
}

for (let int i = 0; i < 3; i = i + 1) {
    print(i);
}
```

## Arrays and conversions

```ylang
let int values[] = [10, 20, 30];
values[0] = 99;

let float number = to_float(values[1]);
let string text = to_string(values[2]);
```

## Standard library

```ylang
let string input = io.read_line();
let bool numeric = string.is_int(input);
let float root = math.sqrt(81.0);
let bool exists = path.exists("notes.txt");
```

Remember: input is text; call `string.is_int` before `string.parse_int` on untrusted input. The math API requires floats. File operations may fail. `path.exists` can return true for a directory or another filesystem object, not only a regular file.

## CLI commands

```sh
ylang --help
ylang --version
ylang check hello.yl
ylang build hello.yl -o hello
ylang build hello.yl -o hello --cc clang
ylang emit-c hello.yl -o build/hello.c
ylang fix broken.yl -o fixed.yl
```

The experimental assembly commands are `ylang emit-asm` and `ylang build-asm`. Use them only when the source is within the NASM backend's current supported subset.

---

# Chapter 26. Practice and Next Steps

Work through these exercises in order. Put each solution in its own `.yl` file. Run `ylang check`, then build and run the program.

1. **Personal greeting:** read a name using `io.read_line()` and print a greeting with an f-string.
2. **Checked integer input:** read a string, call `string.is_int`, parse only after it returns true, and print the value plus one.
3. **Even or odd:** write a function that returns a boolean using `value % 2 == 0`.
4. **Counting:** print one through ten with a `for` loop.
5. **Array total:** create a fixed-size integer array and calculate the sum of its elements.
6. **Geometry:** convert integer measurements to floats and use `math.hypot` to calculate a right triangle's hypotenuse.
7. **Text processing:** read a line and report its byte length and whether it contains a selected word.
8. **File copy:** read a text file, write it to a different path, and report a failed write.
9. **Robust score input:** extend Chapter 18 to reject negative scores as well as invalid text.
10. **Experimental exploration:** once you are comfortable with the stable core, try a struct or an import and read its current reference before using it in a larger program.

When reporting a compiler bug, include the exact version, operating system, compiler, command line, minimal source file, expected behavior, and actual result. A small reproducible example is much easier to investigate than a large application.

Learn one feature at a time. Keep examples small, read diagnostics carefully, make conversions explicit, and test both valid and invalid inputs. Use the stable core for compatibility-sensitive code and treat the extension chapters as guides to experiments.

The goal of this book is not merely to list syntax. It is to help you understand why the code works, how to diagnose the mistakes, and how to grow a small example into a complete program.

# Native Windows support (experimental)

YLang's native Windows work targets 64-bit Windows with the MinGW-w64 toolchain. The Windows CI smoke job builds the compiler, builds the optional SDL2 GUI C runtime, compiles and runs a simple native program, verifies relative multi-file imports, and tests standard input.

This is the first portability milestone, not a claim that every YLang feature has full Windows parity. NASM output remains Linux x86-64-only. MSVC is not a supported build toolchain for this milestone.

## 1. Install MSYS2 and the toolchain

Install MSYS2 from https://www.msys2.org/, then open the **MSYS2 MINGW64** terminal (not the plain MSYS terminal).

Update the package database and install the tools:

    pacman -Syu
    pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-curl mingw-w64-x86_64-pkgconf make python

Ensure the MinGW64 bin directory is on PATH when using YLang from a Windows terminal:

    C:\msys64\mingw64\bin

The exact directory can differ if MSYS2 was installed somewhere other than C:\msys64.

## 2. Build the compiler

Clone the repository from the MINGW64 terminal and build:

    git clone https://github.com/Yassine-Jemi01/ylang.git
    cd ylang
    make clean all CC=gcc
    ./build/ylang.exe --version

Run the regression suite where the command dependencies are available:

    make test CC=gcc

Windows CI currently runs a focused native smoke suite rather than claiming that every POSIX-only shell integration test has been ported.

## 3. Compile a native Windows program

Create hello.yl:

    function main() -> int {
        print("Hello, Windows");
        print("Answer:", 40 + 2);
        return 0;
    }

Then:

    ./build/ylang.exe check hello.yl
    ./build/ylang.exe build hello.yl -o hello.exe
    ./hello.exe

The default C backend generates C and invokes GCC or Clang. Use a compiler executable that is available in the environment's PATH.

## 4. Multi-file imports

Relative imports work from the importing source file:

    from "./math.yl" import answer;

YLang treats Windows paths case-insensitively when deduplicating resolved modules. The module search environment variable uses Windows' semicolon-separated path-list convention:

In the MSYS2 MINGW64 shell, use forward slashes in drive paths:

    export YLANG_PATH='C:/dev/ylang-libs;D:/shared/ylang'

In PowerShell, use:

    $env:YLANG_PATH = "C:/dev/ylang-libs;D:/shared/ylang"

## 5. Optional GUI and Game2D

The compiler can validate GUI calls without SDL2 or libcurl, but GUI builds require SDL2 development files and pkg-config. Programs using the experimental HTTP namespace additionally require the MinGW-w64 libcurl package and pkg-config; see [HTTP and API Requests](HTTP.md). The MINGW64 package command above installs both.

The initial GUI runtime supports one active window, event polling, keyboard/mouse state, timing, background clearing, and filled rectangles. Game2D is intended for simple 2D prototypes and examples. Sprites, audio, in-window text rendering, scene management, and a full physics engine are not part of this milestone.

## Current limitations

- Windows support is experimental and the CI smoke tests cover only a focused path through the compiler/runtime.
- MSVC is not supported by this build recipe; use MinGW-w64 GCC.
- The experimental NASM backend is Linux x86-64-only and does not compile all language features.
- Some developer tests and installation scripts still need platform-specific follow-up.
- Installing YLang system-wide and packaging all optional runtime dependencies on Windows is still future work.

When reporting a Windows issue, include the Windows version, MINGW64 package/toolchain versions, exact command, source files, full output, and whether the problem affects check, generated C compilation, or execution.

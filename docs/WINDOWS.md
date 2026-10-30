# Native Windows support — YLang 3.0 stability target

**Current status:** the implementation is still a focused MinGW-w64 port with smoke-test coverage. It must not be described as stable before the 3.0 acceptance gates pass.

**YLang 3.0 goal:** make 64-bit Windows an officially supported native platform using the documented MinGW-w64 toolchain. The supported setup is MSYS2 MINGW64 with MinGW-w64 GCC. MSVC and Windows ARM64 are outside the 3.0 support promise unless they receive separate compiler, runtime, dependency, linker, and CI coverage.

The current Windows CI smoke job builds the compiler, builds the optional SDL2 GUI C runtime, compiles and runs a simple native program, verifies relative multi-file imports, and tests standard input. This is useful baseline coverage, not yet complete platform parity. NASM remains a deliberately Linux x86-64-specific backend; its stable scope will be published in the 3.0 capability matrix.

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

## Current implementation limits

- The current Windows path is not yet stable; the existing CI job is a smoke suite, not full regression or feature-parity coverage.
- MSVC is not supported by this build recipe; use the documented MinGW-w64 GCC toolchain.
- NASM is intentionally Linux x86-64-only and supports a narrower, explicitly documented subset.
- The interactive C-backend debugger now launches MinGW-w64 GDB; remaining editor/debug-adapter workflows and installation scripts still need Windows-specific implementation and validation.
- System-wide Windows packaging and clean install/upgrade/uninstall tests are not complete.

## YLang 3.0 stable-support acceptance

Before this page can describe Windows support as stable, the 3.0 release candidate must pass:

- A clean compiler build and the complete applicable test suite on a native Windows runner.
- Native build/run tests for compiler-generated executables, imports, paths with spaces, environment variables, standard streams, process exit codes, and filesystem behavior.
- SDL2 GUI/Game2D and libcurl HTTP build/runtime tests using the documented MSYS2 packages.
- Ownership cleanup, error handling, and runtime tests on Windows, including sanitizer-equivalent checks where available.
- Interactive CLI debugging through the documented MSYS2 MINGW64 / MinGW-w64 GDB setup, including a debugger launch test with a source and output path containing spaces.
- LLVM object generation and native linking for the supported Windows target.
- Installation, upgrade, removal, and documentation checks from a clean Windows environment.

The stable support statement will remain limited to Windows x86-64 with MinGW-w64 GCC unless other toolchains/architectures independently pass the same gates. The 3.0 release must not promote this port by changing its label alone.

When reporting a Windows issue, include the Windows version, MINGW64 package/toolchain versions, exact command, source files, full output, and whether the problem affects check, generated C/LLVM compilation, or execution.

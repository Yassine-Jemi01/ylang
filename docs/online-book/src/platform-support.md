# Platform Support

YLang has different support levels on each platform. Installation instructions do not imply the same backend coverage everywhere.

## Linux

Linux is a primary development platform for the C backend and the experimental NASM x86-64 subset. A compatible Linux x86-64 system can also install the optional YSPM package; see [YSPM Linux Packages](reference/YSPM.md).

## macOS

macOS is tested with Clang and GNU Make. Use `gmake` rather than the system BSD `make`. GUI/Game2D programs need SDL2 development files and `pkg-config`.

## Windows

The initial native path targets 64-bit Windows with MinGW-w64 from MSYS2 MINGW64. CI tests compilation, a native executable, relative imports, standard input, and SDL2 build/link. MSVC is not supported by this first milestone, and full Windows feature parity is not claimed.

Read the [Windows Setup](reference/WINDOWS.md) guide for toolchain commands and current restrictions.

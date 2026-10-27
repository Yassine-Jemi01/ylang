# Platform Support

YLang has different support levels on each platform. Installation instructions do not imply the same backend coverage everywhere. The default C-backend workflow depends on GCC or Clang, while optional libraries and experimental backends add their own requirements.

Use this chapter as an overview, then read the linked platform guide and test the features your project actually uses.

## Linux

Linux is a primary development platform for the C backend and the experimental NASM x86-64 subset. GCC and Clang are supported C compiler choices. A compatible Linux x86-64 system can also install the optional YSPM package; see [YSPM Linux Packages](reference/YSPM.md).

For ordinary command-line programs, a C toolchain and Make are the main requirements. GUI/Game2D needs SDL2 development files and \`pkg-config\`. The NASM backend is experimental and supports a narrower set of programs than the C backend.

## macOS

macOS is tested with Clang and GNU Make. Install Apple's command-line developer tools and use Homebrew for GNU Make and optional SDL2 dependencies. Use \`gmake\` rather than the system BSD \`make\` when following repository commands.

GUI/Game2D programs need SDL2 development files and \`pkg-config\`. Verify the native toolchain and run the project's tests in the environment where you intend to build your application.

## Windows

The initial native path targets 64-bit Windows with MinGW-w64 from the MSYS2 MINGW64 environment. CI tests compilation, a native executable, relative imports, standard input, and SDL2 build/link. MSVC is not supported by this first milestone, and full Windows feature parity is not claimed.

Read the [Windows Setup](reference/WINDOWS.md) guide for toolchain commands and current restrictions. A program that builds on Linux may depend on an unsupported feature or library on Windows, so validate the subset your project uses rather than assuming complete portability.

## Backend coverage is separate from OS support

The C backend is the default and broad-coverage route. The NASM backend is experimental and limited to a subset of the language on supported Linux x86-64 systems. A platform having a working compiler does not mean that every backend is available there.

Similarly, a source file passing \`check\` does not guarantee that it can be built by every backend. Use \`build\` for the default C route and only use the experimental assembly commands after checking the [Backend Roadmap](reference/BACKEND-ROADMAP.md).

## A practical portability checklist

Before claiming that an application supports a platform:

1. Build it with the supported toolchain on that platform.
2. Run the relevant unit and regression tests.
3. Verify native dependencies and linking.
4. Test runtime behavior, including file paths, console input, and failure cases.
5. If it uses GUI/Game2D, run it in a real graphical session.
6. Record known unsupported features instead of describing the application as fully portable.

## Report a platform issue

Include the operating system and architecture, compiler version, selected backend, exact build command, full diagnostic output, and a minimal source file that reproduces the problem. This information helps distinguish a language issue from a platform dependency or backend limitation.
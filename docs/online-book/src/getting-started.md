# Install YLang

This chapter explains how to build the YLang compiler from source, verify the build, and prepare the optional dependencies used by graphical examples. Building from source is the most direct way to use the compiler version documented by this book and run the project's regression tests.

## Before you begin

You need:

- Git to download the source repository.
- GCC or Clang with C17 support to compile the YLang compiler and the C it generates.
- GNU Make to run the project's build and test targets.
- A terminal in which you can run shell commands.

Linux and macOS are the primary documented development environments. A native 64-bit Windows path using MinGW-w64 is available, but it remains limited until the YLang 3.0 platform gates pass. The 3.0 goal is official Windows x86-64 support with full native regression coverage, not just a smoke test. See the [Windows Setup](reference/WINDOWS.md) guide for the current state and [3.0 roadmap](reference/ROADMAP-3.0.md) for the acceptance requirements.

GUI and Game2D examples have extra requirements: SDL2 development files and \`pkg-config\`. These are not required for ordinary command-line programs.

## Linux

### Fedora

Install the compiler toolchains and build tools:

\`\`\`sh
sudo dnf install gcc clang make git
\`\`\`

### Debian or Ubuntu

Install the equivalent packages:

\`\`\`sh
sudo apt update
sudo apt install build-essential clang make git
\`\`\`

### Clone and build

Download the repository, enter its directory, and build the compiler:

\`\`\`sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
\`\`\`

The resulting compiler should be located at \`build/ylang\`. If \`make\` fails, read the first compiler or linker error in the output; later messages may only be consequences of that first failure.

Run the regression tests:

\`\`\`sh
make test
\`\`\`

A successful test run is a useful check that the compiler and its expected development dependencies are working. It does not mean every experimental feature works on every platform.

## macOS

Install Apple's command-line developer tools:

\`\`\`sh
xcode-select --install
\`\`\`

Install GNU Make and the LLVM toolchain with Homebrew:

\`\`\`sh
brew install make llvm git
\`\`\`

Use GNU Make as \`gmake\` when invoking the build and test targets:

\`\`\`sh
gmake
gmake test
\`\`\`

For GUI/Game2D programs, install SDL2 and \`pkg-config\` as well:

\`\`\`sh
brew install sdl2 pkg-config
\`\`\`

The compiler's normal C-backend workflow still needs a working C compiler. If a build cannot find GCC or Clang, verify that the compiler is installed and available on your shell's \`PATH\`.

## Windows

The experimental native Windows path uses 64-bit MinGW-w64 from an MSYS2 MINGW64 shell. MSVC is not the supported toolchain for this milestone.

Follow the [Windows Setup](reference/WINDOWS.md) guide for the exact package list, environment, and current limitations. Do not assume that a build tested on Linux will have identical support on Windows.

## Verify the compiler

After \`make\` finishes, check that the compiler executable exists:

\`\`\`sh
./build/ylang
\`\`\`

The compiler's help output should list the available commands. The everyday workflow is:

\`\`\`sh
./build/ylang check path/to/program.yl
./build/ylang build path/to/program.yl -o program

./build/ylang run path/to/program.yl
\`\`\`

Replace the example path with an actual \`.yl\` file. The first command checks the source; the second creates a native executable through the default C backend.

## Optional: GUI and Game2D dependencies

On Fedora, SDL2 development files and \`pkg-config\` can be installed with:

\`\`\`sh
sudo dnf install SDL2-devel pkgconf-pkg-config
\`\`\`

On Debian or Ubuntu:

\`\`\`sh
sudo apt install libsdl2-dev pkg-config
\`\`\`

On macOS, use the Homebrew command shown above. If the graphical example fails to compile, verify that SDL2's development files are installed and that \`pkg-config\` can find them. If it builds but no window appears, run it from a graphical desktop session; headless environments cannot validate live window behavior.

## Optional: install a Linux package

YLang also publishes an optional \`.yspkg\` package for compatible Linux x86-64 systems using YSPM. This is a packaging option, not a replacement for the compiler toolchain required when building from source. See [YSPM Linux Packages](reference/YSPM.md) for package requirements and installation details.

## Troubleshooting

- **\`make: command not found\`:** install GNU Make, or use \`gmake\` on macOS.
- **\`gcc\` or \`clang\` is missing:** install one of the supported C toolchains and check that it is available on \`PATH\`.
- **SDL2 is not found:** install the development package, not only the runtime library, and confirm that \`pkg-config\` is installed.
- **A test fails:** preserve the first failing test's output and command. Do not treat a partial test run as a complete pass.
- **A program builds on one platform but not another:** check the platform support page and the relevant backend contract before assuming the source is portable.

## Next

Continue to [Your First Program](hello-world.md) to create a source file, check it, build a native executable, and run it.
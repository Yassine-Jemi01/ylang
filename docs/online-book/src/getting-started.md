# Install YLang

This chapter builds YLang from source. It is the most direct way to use the compiler and run the regression tests.

## Requirements

- Linux, macOS, or the experimental 64-bit Windows path described in [Platform Support](platform-support.md).
- GCC or Clang with C17 support.
- GNU Make (`make` on Linux; `gmake` on macOS).

GUI and Game2D programs also need SDL2 development files and `pkg-config`. Those packages are optional for ordinary command-line programs.

## Linux

On Fedora:

```sh
sudo dnf install gcc clang make
```

On Debian or Ubuntu:

```sh
sudo apt install build-essential clang make
```

Then clone and build the project:

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
make
make test
```

The compiler is written to `build/ylang`.

## macOS

Install Apple's command-line tools and the build dependencies:

```sh
xcode-select --install
brew install make llvm
```

Use GNU Make as `gmake` for the build and test commands:

```sh
gmake
gmake test
```

For GUI/Game2D builds, also install SDL2 and `pkg-config`:

```sh
brew install sdl2 pkg-config
```

## Windows

The initial native Windows target uses 64-bit MinGW-w64 from an MSYS2 MINGW64 shell. MSVC is not the supported toolchain for this milestone. Follow the [Windows setup guide](reference/WINDOWS.md) for exact packages and limitations.

## Linux package option

YLang also provides an optional `.yspkg` package for compatible Linux x86-64 systems using YSPM. YSPM is the system package manager; it is not a project-local YLang module manager. See [YSPM Linux Packages](reference/YSPM.md).

## Next

Continue to [Your First Program](hello-world.md) to check, compile, and run a `.yl` file.

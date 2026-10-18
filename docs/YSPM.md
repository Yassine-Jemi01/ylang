# YSPM integration for YLang on Linux

[YSPM](https://github.com/Yassine-Jemi01/yspm) is a Linux **system package manager**. It installs native system packages in the `.yspkg` format. It is not a package manager for importing `.yl` modules into an individual YLang project.

YLang's release pipeline can build a native `.yspkg` so Linux users of a compatible YSPM system can install the compiler and its source-level standard library through YSPM. This is an optional Linux distribution path; normal source builds remain available on Linux/macOS, and native Windows follows the MinGW-w64 guide.

## Build a .yspkg locally

Requirements:

- Linux x86-64 with a C compiler and GNU Make.
- Go 1.23 or newer to build YSPM from source.
- A YSPM version that supports the native .yspkg build format and the `yspm build` command.

Build YSPM and then the YLang package from the repository root:

From a workspace with sibling ylang and yspm directories:

    cd ..
    git clone https://github.com/Yassine-Jemi01/yspm.git
    cd yspm
    go build -o yspm ./cmd/yspm
    cd ../ylang
    YSPM_BIN="../yspm/yspm" packaging/yspm/build-package.sh

The script builds YLang for `/usr`, stages the compiler, .yl standard-library files, GUI runtime sources, public GUI header, license, and available documentation, then asks YSPM to produce:

    build/ylang-yspm/ylang-<version>-amd64.yspkg

The package builder defaults to ABI `yspm-abi-1` and architecture `amd64`. Override these only to match the target YSPM repository/system. The script defaults to static linking for the compiler executable to reduce undeclared runtime shared-library requirements; set `YLANG_PACKAGE_LDFLAGS` only when intentionally building a different variant.

## Install a published package

When a .yspkg asset is published for a YLang release, download that asset and install it with YSPM:

    sudo yspm install ./ylang-<version>-amd64.yspkg

The exact package filename depends on the release version. The package can also be built locally using the command above.

The system package installs the `ylang` executable in `/usr/bin`, source modules in `/usr/share/ylang/stdlib`, the optional GUI runtime source, the GUI public header, and documentation under `/usr/share/doc/ylang`.

For named imports of installed standard-library source modules, add the shared standard-library directory to YLANG_PATH when needed:

    export YLANG_PATH=/usr/share/ylang/stdlib

## Important boundaries

- YSPM manages Linux **system packages**, not project-local YLang module dependencies.
- The .yspkg is a Linux x86-64 package intended for a compatible YSPM ABI. It is not a macOS or Windows installer.
- The package contains the YLang compiler and source modules, but does not automatically install GCC/Clang, GNU Make, SDL2 development files, or pkg-config. `ylang build` compiles generated C and therefore requires GCC or Clang on the host. GUI/game builds additionally require SDL2 development dependencies and pkg-config.
- The YSPM repository catalog and release signing are managed in the YSPM repository. Publishing a .yspkg as a YLang release asset does not automatically add it to every YSPM repository index.
- A YSPM package is not considered published until the corresponding release asset has been built and attached successfully by CI.

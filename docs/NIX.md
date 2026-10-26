# Nix Flakes support

YLang includes an initial Nix Flake for building the compiler and entering a repeatable Linux development environment.

**Current scope:** `x86_64-linux`. The Nix package builds the compiler from this checkout, runs the existing Makefile test suite, and installs the standard-library sources and GUI headers/runtime source. Nix packaging is new and should be treated as experimental until it has been tested on NixOS by maintainers.

The flake commits `flake.lock`, which pins the exact Nixpkgs snapshot used by builds. To intentionally update the Nixpkgs snapshot later, run `nix flake update`, review the `flake.lock` diff, and commit the reviewed change.

## Enable Flakes

If your Nix installation has not enabled Flakes yet, on NixOS add this to your system configuration:

```nix
nix.settings.experimental-features = [ "nix-command" "flakes" ];
```

Then rebuild the system, or enable the features temporarily for an individual command:

```sh
nix --extra-experimental-features "nix-command flakes" develop
```

## Build and run

From the root of a YLang checkout:

```sh
nix build --print-build-logs
./result/bin/ylang --version
```

The default package runs the Makefile's test suite during the build. To run the compiler without manually referring to the result symlink:

```sh
nix run . -- --version
```

To compile a program, use the same CLI as the regular source build:

```sh
nix run . -- check hello.yl
nix run . -- build hello.yl -o hello
./hello
```

The packaged compiler sets `YLANG_PATH` to its installed standard-library directory and adds GCC, Clang, NASM, and binutils to its runtime `PATH`. That allows the compiler to invoke a native C compiler for generated programs and provides the tools used by the experimental NASM backend. SDL2 is not bundled into the compiler package; use `nix develop` for GUI/Game2D builds, where SDL2 and pkg-config are available.

## Development shell

Enter a shell with the C toolchain, GNU Make, Python, SDL2, pkg-config, NASM, and binutils:

```sh
nix develop
make
make test
```

The development shell sets `YLANG_PATH=./stdlib`, so named imports can find the source modules in this checkout. GUI/Game2D development dependencies are available in the shell; the corresponding YLang APIs remain experimental.

## Install the package in a Nix profile

A local checkout can be installed with:

```sh
nix profile install .
ylang --version
```

For now, this flake packages the current repository version. The repository's `VERSION` may identify a development candidate; it does not change which YLang release is considered the latest published stable release. Check the [release page](https://github.com/Yassine-Jemi01/ylang/releases) before treating a source build as a stable release.

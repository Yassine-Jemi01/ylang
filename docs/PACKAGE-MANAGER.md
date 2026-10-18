# YLang package manager (ylpm)

ylpm is an experimental project and dependency manager for YLang. This initial version supports explicitly tagged GitHub repositories as source packages. It does not yet provide a public package registry or dependency version solver.

## Install or build ylpm

The source is in tools/ylpm and uses the Go standard library only. With Go 1.23 or newer:

    cd tools/ylpm
    go test ./...
    go build -o ylpm .

On Windows, the resulting executable is ylpm.exe. CI tests the package manager on Linux, macOS, and Windows.

## Create a project

Run from the directory that should contain the project:

    ylpm init my-game

This creates ylang.json and a starter main.yl. A minimal manifest looks like:

    {
      "name": "my-game",
      "version": "0.1.0",
      "entry": "main.yl",
      "dependencies": {}
    }

## Add and install a package

Package source strings must specify a GitHub owner, repository, and tag:

    ylpm add math github:example/ylang-math@v1.0.0
    ylpm install
    ylpm list
    ylpm remove math

The example repository is illustrative; replace it with a real repository that publishes YLang source files.

Packages are installed into yl_modules/<package-name>/. Commit both ylang.json and ylpm.lock. The lock records the selected source, archive URL, and SHA-256 digest. Future installs compare the downloaded archive with the stored digest and fail if it changed.

## Build and run

    ylpm build
    ylpm build --output build/my-game
    ylpm run

By default, ylpm finds ylang on PATH. Set YLANG_COMPILER if the compiler executable has a custom location. The manager adds directories containing .yl files in installed packages to YLANG_PATH for the build and run commands.

## Security and trust

Downloads must use HTTPS. Archives are limited to 64 MiB, extracted source is limited to 256 MiB, and archives with more than 10,000 entries are rejected. Extraction rejects absolute paths, traversal segments, Windows path tricks, symlinks, special files, multiple top-level roots, and packages without .yl source files. Files are extracted into a staging directory before the package is activated.

The SHA-256 lock detects unexpected archive changes between installs; it is not a publisher signature, and it does not prove that a package author is trustworthy. Read a dependency's source and use repositories you trust before compiling or running third-party code.

## Current limitations

- No central registry or package search command.
- No version ranges, dependency solver, or transitive version resolution.
- No package signing or publisher identity verification.
- One package name maps to one explicitly tagged GitHub repository.
- Dependency updates must be requested explicitly by selecting another tag.
- The package-manager interface and manifest format may change before stable YLang 2.0.

The module manager is separate from the experimental NASM backend: installed packages are usable only when the chosen compiler backend supports the language constructs they use. The C backend remains the default and broadest path.

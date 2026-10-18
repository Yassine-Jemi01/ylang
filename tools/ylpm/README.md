# ylpm — YLang package manager

ylpm is an initial cross-platform package manager for YLang projects. This is an early version, not a mature package registry.

## Commands

Build from this directory with Go 1.23 or newer:

    go test ./...
    go build -o ylpm .

Initialize a project:

    ylpm init my-game

This creates ylang.json and a starter main.yl. A minimal manifest looks like:

    {
      "name": "my-game",
      "version": "0.1.0",
      "entry": "main.yl",
      "dependencies": {}
    }

Add a package from a tagged GitHub source. The tag must be explicit:

    ylpm add math github:example/ylang-math@v1.0.0
    ylpm install
    ylpm list
    ylpm remove math

Packages are installed under yl_modules/<name>/. Keep ylang.json and ylpm.lock in version control. The lockfile records the archive URL and SHA-256 digest; "ylpm install" checks the digest of any package already locked.

Build and run through the manager:

    ylpm build
    ylpm build --output build/my-game
    ylpm run

The manager finds ylang on PATH by default. Set YLANG_COMPILER to a custom compiler path when needed. "ylpm run" passes remaining command-line arguments to the program.

## Package source format

The first implementation accepts only github:owner/repository@tag sources and downloads GitHub's source ZIP over HTTPS. There is no central registry or dependency solver yet. Tags are required for reproducibility, and the SHA-256 lock protects future installs from archive changes. The lock is an integrity check, not a publisher signature; use packages and repositories you trust.

The ZIP extractor rejects traversal paths, absolute paths, Windows path tricks, symbolic links, unsupported file types, archives with multiple roots, packages without .yl sources, and archives that exceed the download or unpack limits. Packages are unpacked in a staging directory before being activated.

## Importing package modules

ylpm adds directories containing .yl files under yl_modules to YLANG_PATH for "ylpm build" and "ylpm run". A package can therefore expose source files that are imported by the standard YLang named-import syntax. Check a package's documented entry file and import paths before depending on its API.

## Current limitations

- No central package index, version ranges, transitive dependency solver, or package signing yet.
- One named package maps to one explicitly tagged GitHub repository source.
- Dependency updates are explicit; select a new tag with "ylpm add <name> github:owner/repo@tag".
- The package manager does not claim that downloaded source code is safe. Review third-party packages before compiling or running them.
- ylpm is implemented in Go and is designed for Linux, macOS, and Windows. CI smoke tests and user testing are still required before calling it stable.

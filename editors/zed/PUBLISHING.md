# Publishing YLang in the Zed Extension Gallery

The Zed extension registry accepts extensions stored in a repository subdirectory.
YLang's extension lives in `editors/zed`, so a registry entry can use the
`path = "editors/zed"` field.

## Before submitting

1. Build the extension with the same Zed release you intend to support.
2. Install it in Zed using **zed: install dev extension** and select
   `editors/zed`.
3. Verify that a `.yl` file is detected, highlighted, and that the LSP starts
   when `ylang-lsp` is installed.
4. Verify the missing-LSP message when `ylang-lsp` is not on `PATH`.
5. Confirm that the checked-out grammar revision in `extension.toml` exists
   on a public branch in this repository.
6. Keep `editors/zed/LICENSE` in place. The Zed registry requires an accepted
   license inside the extension directory; this extension uses MIT.

## Registry submission

Follow the current official Zed publishing guide:
https://zed.dev/docs/extensions/publishing/publishing-guide

The registry change consists of:
- Adding this repository as a Git submodule at `extensions/ylang`, using its
  HTTPS URL and a branch-backed commit.
- Adding the following entry to the registry's top-level `extensions.toml`:

```toml
[ylang]
submodule = "extensions/ylang"
path = "editors/zed"
version = "0.2.0"
```

- Running `pnpm sort-extensions` in the registry repository.
- Opening a pull request against `zed-industries/extensions`.

The Zed extension registry has an AI policy requiring a human contributor who
understands the submission and writes their own communication with maintainers.
The maintainer-facing pull request description and replies must therefore be
written by the project maintainer, not generated or submitted by an autonomous
agent. See https://github.com/zed-industries/extensions/blob/main/AI_POLICY.md.

## After acceptance

Once the registry pull request is reviewed, accepted, and merged, Zed will
package and publish the extension. Users will then be able to search for
`YLang` in Zed's Extension Gallery.

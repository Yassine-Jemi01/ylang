# YLang 2.2.0 Candidate Acceptance

**Do not publish 2.2.0 until this test has passed on the final candidate commit and the project owner has reviewed the behavior.**

The repository's GitHub Actions matrix tests GCC, Clang, macOS, Windows native smoke behavior, sanitizers, HTTP/async requests, source mapping and debugger launch, editor diagnostics, package contents, and the refreshed website. This local script consolidates the same acceptance work into one command:

```sh
bash tools/test-2.2-candidate.sh
```

It checks the normal compiler/regression suite, the HTTP/async integration fixture, the source-aware debugger path, a repeatable benchmark with known output, ASan/UBSan, VS Code tests and a packaged `.vsix`, Neovim tests when installed, the mdBook output and link integrity, and the React homepage build. The extension package is written under `build/acceptance/`. It requires GCC or Clang, Make, Python 3, pkg-config plus libcurl development files, GDB or LLDB, Node.js 22+, npm, and mdBook 0.5.4.

On Fedora, install the common build dependencies with:

```sh
sudo dnf install gcc clang make python3 pkgconf-pkg-config libcurl-devel gdb nodejs npm
```

Install mdBook 0.5.4 from its official installation instructions if it is not available in the system package sources: https://rust-lang.github.io/mdBook/guide/installation.html

## Run the candidate without disturbing your current checkout

From your existing YLang clone, use a separate Git worktree so your current branch and uncommitted files stay untouched:

```sh
git fetch origin feature/2.2-implementation-mvp
git worktree add ../ylang-2.2-acceptance origin/feature/2.2-implementation-mvp
cd ../ylang-2.2-acceptance
bash tools/test-2.2-candidate.sh
```

For Clang, run the same script with `NATIVE_CC=clang`. After it passes, install the extension package in VSCodium with `codium --install-extension build/acceptance/ylang-language-support-0.6.0.vsix` (or use `code --install-extension ...` in VS Code), then reload the window and open the sample file. If the script fails, keep the full output and report the first failing step rather than publishing the candidate.

Passing CI is necessary but not enough. The project owner should also try the actual public-facing workflow once: run a small `.yl` program that starts two HTTP tasks, awaits them, checks the response/status, then launch `ylang debug` and set a source-line breakpoint. This final hands-on check must use the exact commit proposed for release.

The script builds and tests only; it does not push, merge, tag, deploy, or publish any release.

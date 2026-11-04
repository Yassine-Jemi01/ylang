# YLang command-line reference

The compiler uses explicit subcommands. Run `ylang help`, `ylang --help`, or `ylang -h` to print the concise command list; run `ylang version`, `ylang --version`, or `ylang -V` to print the compiler version. Each form is an alias and produces the same output.

## Commands

| Command | Purpose | Default output |
|---|---|---|
| `ylang check FILE.yl` | Parse and type-check a source file without invoking a native compiler. | Diagnostics only, plus a success line when valid |
| `ylang build FILE.yl [-o PATH] [--cc gcc|clang]` | Generate C and compile it into a native executable. | `a.out` |
| `ylang debug FILE.yl [-o PATH] [--cc gcc|clang]` | Build with debug symbols, then launch GDB on Linux or LLDB on macOS. | `a.out` |
| `ylang emit-c FILE.yl [-o PATH]` | Write generated C for inspection. | `build/ylang-generated.c` |
| `ylang emit-asm FILE.yl [-o PATH]` | Emit experimental NASM for the supported subset. | `build/ylang-generated.asm` |
| `ylang build-asm FILE.yl [-o PATH]` | Assemble and link the experimental NASM subset. | `a.out` |
| `ylang fix FILE.yl -o PATH` | Apply only high-confidence fixes to a separate output file. | Explicit path required |

The `--cc` option selects GCC or Clang for the generated C program. It does not select the compiler used to build YLang itself. The C backend remains the default; NASM is limited to Linux x86-64 and its documented supported subset.

The `fix` command never overwrites the input implicitly. The current automatic-fix set is intentionally narrow; after it writes a file, run `ylang check` on that output.

## Exit behavior

- `0`: requested command completed successfully.
- `64`: command-line usage error or an explicitly unsupported command/platform path.
- `65`: source contains an embedded NUL byte and is rejected as non-text input.
- `66`: the source file could not be read.
- `70`: an allocation/resource preparation failure was reported by the CLI.
- `73`: an output file or temporary build directory could not be created/opened.
- `74`: writing or closing the output produced by `ylang fix` failed.
- `check` and source-validation failures return a nonzero status. A native compiler's exit status is propagated when possible; signal termination is converted to a nonzero status.

Detailed diagnostics are written to standard error. Successful `check`, C generation, assembly generation, and native builds print the output path or source checked. The exact wording may improve, but scripts should rely on the exit code and produced file rather than parsing prose.

## Examples

```sh
ylang help
ylang --help
ylang -h
ylang version
ylang --version
ylang -V
ylang check hello.yl
ylang build hello.yl -o hello
ylang build hello.yl -o hello --cc clang
ylang emit-c hello.yl -o build/hello.generated.c
ylang fix typo.yl -o typo.fixed.yl
ylang check typo.fixed.yl
```

Build and feature limitations are described in the [README](../README.md), [language specification](language-spec.md), and [backend roadmap](BACKEND-ROADMAP.md).

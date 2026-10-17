# YLang 2.0.0-dev

**A compiler-first language experiment built around explicit types, explicit ownership moves, checked operations, and helpful diagnostics.**

YLang has its own compiler front-end in C: lexer, parser, semantic analysis, diagnostic engine, and YLang-specific code generation. For now, its backend emits C17 and invokes GCC or Clang to produce a native executable. This is a dedicated YLang compiler, but not yet a standalone machine-code backend.

> **Development preview:** version 2.0.0-dev is experimental. The stable `main` branch remains YLang 1.1.0. Do not use this preview for security-critical software or as a hardened sandbox.

## Design philosophy

- **Clarity beats magic.** Explicit types and source-located diagnostics keep behavior understandable.
- **Moving is visible.** Strings and arrays move when assigned or passed by value; use `clone(value)` when a separate copy is required.
- **The compiler does the checking.** Type, initialization, move-state, borrow-conflict, and array-index rules are checked where the current implementation can prove them.
- **Fast native programs without a mandatory GC.** GCC or Clang optimize the generated C; array operations and integer arithmetic have runtime checks.
- **Portable first.** Language behavior is implemented against C standard-library APIs when possible. Windows is tested using MSYS2 UCRT64 / MinGW-w64 in CI.
- **Useful core, growing libraries.** A small set of built-ins covers printing, line input, strings, and arrays. Modules, broad file APIs, and a package ecosystem are future work, not hidden features.

These are project principles, not a claim of Rust-equivalent memory safety. The current move/borrow checker is an early implementation and the ownership cleanup model still needs work.

## What is in this preview

| Feature | Current status |
| --- | --- |
| `int`, `float`, `bool`, `char`, `string`, functions, return types | Implemented |
| `if` / `else`, `loop()`, `break`, `continue`, C-style `for` | Implemented; regression-tested |
| Typed one-dimensional arrays, indexing, `len`, `append`, `clone` | Implemented; bounds checked at runtime |
| `input()`, `input_int()`, `input_float()` | Implemented; line-based |
| Scalar shared/exclusive parameters: `&T`, `&mut T` | Initial subset only |
| String/array moves and detected use-after-move | Initial checker; still has known limitations |
| GCC/Clang C17 backend | Implemented |
| Windows build and regression suite | CI-tested; see current workflow status |
| Experimental HTTP/HTTPS REST client (`http_get`, `http_post`, `http_request`) | Initial synchronous client; requires libcurl when used |
| JSON parser/accessors (`json_get_*`, `json_array_*`, `json_quote`) | Initial checked runtime; not yet a full JSON value model |
| Modules/imports, generics, nested arrays, typed error values, broad file/network APIs | Not implemented yet |
| Direct LLVM/machine-code backend | Not implemented; current backend emits C |

See [the v2 language specification](docs/language-spec.md), [ownership and borrowing notes](docs/ownership-and-borrowing.md), [the HTTP API client](docs/http-api.md), [the JSON helper API](docs/json-api.md), and [the development roadmap](docs/roadmap.md).

## Quick start

### Requirements

- Linux (Fedora or Ubuntu) or Windows via MSYS2 UCRT64 / MinGW-w64.
- C17 compiler, Make, and GCC or Clang to compile the generated C.
- Node.js/npm only if you are working on the VS Code extension or Tree-sitter grammar.

Fedora:

```sh
sudo dnf install gcc make clang
```

Debian/Ubuntu:

```sh
sudo apt install build-essential clang make
```

### Build the v2 development branch

```sh
git clone https://github.com/Yassine-Jemi01/ylang.git
cd ylang
git switch v2/http-api-client
make clean
make
make test
```

The compiler is written to `build/ylang` on Linux and `build/ylang.exe` on Windows.

### Example: arrays, input, and loops

Save as `hello.yl`:

```ylang
function main() -> int {
    print("What is your name?");
    let string name = input();

    let int[] scores = [10, 20, 30];
    scores = append(scores, 40);

    for (let int i = 0; i < len(scores); i = i + 1) {
        print(f"{name}'s score {i}: {scores[i]}");
    }
    return 0;
}
```

Check, build, and run:

```sh
./build/ylang check hello.yl
./build/ylang build hello.yl -o hello
./hello
```

The input runtime is line-based and currently limits one input line to 1 MiB. An array index outside the valid range produces a runtime error rather than an unchecked C access.

### Ownership in a small example

```ylang
let string first = "YLang";
let string second = first;      // move first into second
// print(first);                // rejected: first was moved

let string copy = clone(second);
print(second, copy);
```

This is the current ownership direction, not a complete Rust-style borrow checker. In particular, borrowing is limited to scalar function parameters, and cleanup on every early control-flow exit still needs improvement. Review the documented limits before relying on it.

## CLI reference

```text
ylang --help
ylang --version
ylang check <file.yl>
ylang build <file.yl> [-o executable] [--cc gcc|clang]
ylang run <file.yl> [--cc gcc|clang]
ylang emit-c <file.yl> [-o generated.c]
ylang fix <file.yl> -o <fixed.yl>
```

- `check` parses and performs semantic/type checks without running a native compiler.
- `build` checks the source, emits temporary C17, then invokes GCC or Clang.
- `run` compiles into a temporary directory, runs the native program, cleans up temporary artifacts, and returns the program's exit status.
- `emit-c` writes generated C for inspection.
- `fix` applies a narrow, high-confidence fix for a common `pritn(...)` typo. It writes to a separate file and is not a general repair engine.

## Experimental HTTP/HTTPS API client

The `v2/http-api-client` branch adds a synchronous REST client for applications that consume external HTTP APIs. It is experimental and is not yet a namespaced `http` module.

Generated programs that use these functions need the libcurl development package and linker library. Programs that do not call the HTTP functions still build without a libcurl dependency.

Fedora:

```sh
sudo dnf install libcurl-devel
```

Debian/Ubuntu:

```sh
sudo apt install libcurl4-openssl-dev
```

Example:

```ylang
function main() -> int {
    let string body = http_get("https://api.example.com/health");
    let int status = http_status_code();

    if (status >= 200 and status < 300) {
        print(body);
    } else {
        print("HTTP status:", status);
        print(body);
        if (status == 0) {
            print(http_last_error());
        }
    }
    return 0;
}
```

Available prototype calls:

- `http_get(url)` and `http_get_with_headers(url, headers)`
- `http_post(url, body)` and `http_post_with_headers(url, body, headers)`
- `http_request(method, url, body, headers)` for GET, POST, PUT, PATCH, DELETE, and HEAD.
- `http_status_code()` and `http_last_error()` for the most recent request.

The headers argument is a string containing `Name: value` lines separated by line feeds. HTTP response bodies arrive as strings. The initial JSON helper API can read scalar properties, retrieve raw nested objects/arrays, iterate array items, and quote a string; it does not yet expose a general-purpose typed JSON value. Check status immediately after each request: the status and error accessors are process-global in this prototype, not a response object. See [JSON helper API](docs/json-api.md).

The client verifies TLS certificates and hostnames, restricts requests and redirects to HTTP/HTTPS, has 5-second connect and 15-second total timeouts, and limits response bodies to 10 MiB. HTTP status codes such as 404 are responses, not transport failures; inspect the status separately. This is not an HTTP server framework and does not yet provide async requests, retries, WebSockets, or automatic JSON decoding.

See [HTTP API design and limitations](docs/http-api.md).

## Platform support

- Linux: GCC and Clang are exercised in GitHub Actions.
- Windows: the project uses MSYS2 UCRT64 / MinGW-w64. CI builds the compiler, runs the regression suite, runs the native PowerShell smoke test, and checks the Tree-sitter DLL and PowerShell installer.
- MSVC is not a supported generated-C toolchain in this preview.
- VS Code packaging and LSP syntax checks run on both Linux and Windows; editor features are currently basic.
- Tree-sitter grammar tests are part of CI. Neovim integration exists, but grammar regeneration and full editor behavior must be checked along with the compiler when syntax changes.

See [Windows setup](docs/windows.md).

## Open-source tooling and dependencies

The compiler/runtime currently rely on the C standard library instead of requiring a large external native runtime. This reduces packaging and cross-platform risks while the language core is changing.

The project also uses open-source tooling for editor support, including Tree-sitter and the VS Code Language Server Protocol packages. Their generated parser/extension packages are checked in CI. Additional runtime libraries should be adopted only when a concrete standard-library feature needs them and their licensing, version pinning, Windows support, and tests can be maintained.

## Development and safety

```sh
make clean
make
make test
make CC=clang test
make sanitize
```

`make sanitize` uses available compiler sanitizers. Tests cover successful output, diagnostics, invalid programs, safe fixes, checked arithmetic, arrays, bounds failures, moves, input, borrow rules, and editor grammar parsing. Sanitizers and tests help catch defects but do not prove compiler or generated-code memory safety.

## Project resources

- [v2 language specification](docs/language-spec.md)
- [Compiler architecture](docs/architecture.md)
- [YLang design philosophy and roadmap](docs/roadmap.md)
- [Ownership and borrowing](docs/ownership-and-borrowing.md)
- [Memory and arrays](docs/memory-and-arrays.md)
- [Windows setup guide](docs/windows.md)
- [Tree-sitter grammar](tree-sitter-ylang/README.md)
- [Neovim integration](editors/neovim/README.md)
- [VS Code language support](editors/vscode/README.md)

YLang is distributed under the MIT License. See [LICENSE](LICENSE).

# YLang GUI API Contract (Proposal)

Status: initial implementation is in PR #34. The API below is the supported first binding surface for that change; do not treat it as released until the PR passes CI and merges.

This document defines a deliberately small first binding milestone. The contract must be agreed before changing the parser, semantic checker, C generator, and native build driver together.

## Goals

- Make a small graphical window usable from ordinary YLang source.
- Preserve the current default build: programs without `gui.*` calls must not need SDL2.
- Keep `check` independent of SDL2 installation; dependency discovery belongs to native `build`.
- Validate argument count and types before code generation.
- Keep the API portable across the project's supported Linux and macOS targets.
- Make unsupported calls fail with YLang diagnostics rather than producing broken C.

## Proposed first API

The first milestone intentionally supports one active window per process. It does not expose raw pointers or arbitrary C interop.

| Call | Return type | Contract |
| --- | --- | --- |
| `gui.init()` | `bool` | Initialize the GUI runtime; false means initialization failed. |
| `gui.create_window(title, width, height)` | `bool` | Create one window; title is `string`, dimensions are positive `int` values. |
| `gui.poll()` | `bool` | Process pending events; false means the window was closed or polling failed. |
| `gui.clear(red, green, blue)` | `bool` | Clear the current frame using RGB channels in the range 0–255. |
| `gui.draw_rect(x, y, width, height, red, green, blue)` | `bool` | Draw a filled rectangle; dimensions must be positive and color channels 0–255. |
| `gui.present()` | `bool` | Present the current frame. |
| `gui.last_error()` | `string` | Return a readable description of the latest GUI runtime error. |
| `gui.close()` | `void` | Destroy the active window and release its resources. Safe to call after failed creation. |
| `gui.shutdown()` | `void` | Release initialized GUI runtime resources. |

All calls except `gui.close()` and `gui.shutdown()` require successful initialization and, where applicable, a created window. Runtime failures are returned as `false`; the latest human-readable explanation is available through `gui.last_error()`. No implicit numeric conversions are allowed.

## Example target syntax

This example shows the intended use of the API and is included in the implementation work.

```ylang
function main() -> int {
    if (not gui.init()) {
        print(gui.last_error());
        return 1;
    }

    if (not gui.create_window("YLang GUI", 800, 500)) {
        print(gui.last_error());
        gui.shutdown();
        return 1;
    }

    loop() {
        if (not gui.poll()) { break; }
        if (not gui.clear(24, 28, 38)) { break; }
        if (not gui.draw_rect(80, 80, 240, 120, 22, 160, 180)) { break; }
        if (not gui.present()) { break; }
    }

    gui.close();
    gui.shutdown();
    return 0;
}
```

The final example must be added to executable regression tests only after the compiler and build driver support the API.

## Compiler and build requirements

1. **Parser:** accept `gui` as a recognized standard-library namespace using the same qualified-call representation as existing namespaces.
2. **Semantic analysis:** define an explicit signature table for every supported call; check arity and each argument's type; assign the documented return type. Unknown `gui.*` names must produce a stable diagnostic.
3. **C generation:** emit calls to a narrow adapter layer. Do not pass arbitrary user-selected C symbols through to generated code.
4. **Native build driver:** detect whether the checked program uses GUI calls. Only then discover SDL2 compiler/linker flags and link the optional runtime. Use argument-vector process execution, not a shell command string.
5. **Portability:** account for the repository-relative runtime source and headers, and report a clear build error if SDL2 development files are missing. The ordinary non-GUI build must continue working without SDL2.
6. **Experimental NASM backend:** reject GUI calls with a clear unsupported-feature diagnostic; do not silently generate invalid assembly.
7. **Documentation:** clearly label this as an optional SDL2 feature and distinguish compile-time checking from dependency-required native building.

## Acceptance tests

- `check` accepts a well-typed GUI program even on a machine without SDL2 installed.
- `check` rejects incorrect arity, wrong argument types, unknown GUI names, and invalid types with source-located diagnostics.
- A regular non-GUI program builds and runs with no SDL2 dependency.
- A GUI program builds on Linux and macOS when SDL2 development files are installed.
- A GUI build with missing SDL2 reports the required package/tooling and exits nonzero.
- Runtime initialization/window-creation failure is reportable through `gui.last_error()`.
- The existing C GUI example and all compiler regression tests remain passing.
- The NASM backend rejects GUI programs before assembly generation.

## Deliberate non-goals for the first milestone

No widget toolkit, text input, images, audio, game loop abstraction, multiple-window handles, custom event objects, arbitrary FFI, or Pygame-like game framework. Those should build on the first API only after its ownership, error, and lifecycle behavior is tested.

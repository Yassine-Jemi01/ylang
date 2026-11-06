# YLang Debugger Design

**Status: design proposal for the YLang 2.2 roadmap. No debugger command or VS Code debug configuration should be advertised as implemented until this specification is accepted and the implementation passes its tests.**

## Goal

Let a developer debug a .yl file from VS Code or the command line, seeing YLang source locations and meaningful variable names rather than generated C internals.

## Initial scope

The first implementation should target the established C backend. Support for the NASM backend is out of scope until it can emit and verify appropriate debug information.

The first milestone should aim to provide:
- Build and launch a YLang program with debug symbols.
- Set source-line breakpoints, continue, step over, step into, and step out.
- Show the current YLang source location and call stack.
- Inspect supported local scalar variables.
- Display clear errors when the compiler, native debugger, symbols, or source mapping are unavailable.

Inspection of arrays, structs, owned values, optimized code, and generated runtime internals should be added only when each representation can be exposed reliably.

## Compiler and source mapping

1. Add an explicit debug build mode; do not silently change the normal optimized build.
2. Compile generated C with debug symbols and debugging-friendly optimization settings.
3. Preserve a mapping between YLang source files/lines and generated C. A prototype should evaluate generated-C #line directives and any additional source-map metadata needed for expressions or locals.
4. Preserve source file identity for relative imports and files outside the current working directory.
5. Keep temporary generated sources and binaries in a predictable, documented location, and make cleanup behavior explicit.
6. Do not rely on C variable names alone as the permanent YLang debugger interface.

A source-level breakpoint must land on a valid executable statement. If a line contains only a declaration, comment, or compound construct with no instruction location, the adapter should explain or resolve the breakpoint rather than claiming that it was bound exactly.

## Debug-engine choice

Before settling the public interface, implement a small proof of concept and compare:

- **Reuse the VS Code C/C++ debugger:** generate appropriate launch configuration and source mapping, with a clearly declared dependency on the C/C++ extension.
- **Provide a YLang debug adapter:** implement the Debug Adapter Protocol and communicate with a supported native debugger such as GDB/LLDB.

Choose based on reliable YLang source stepping, local-variable inspection, cross-platform behavior, installation complexity, and maintenance cost. Do not commit the extension's public debugger type until the prototype shows that generated-C details can be hidden adequately. Never invoke a shell command assembled from user-controlled source paths or program arguments.

## VS Code user experience

The desired flow is:
1. Open a YLang workspace and select a YLang debug configuration.
2. Press F5; the extension checks required tools and builds a debug executable if necessary.
3. Set breakpoints in the .yl editor and use the usual Run and Debug controls.
4. See the call stack, supported locals, and console output in the normal VS Code panes.
5. Receive actionable setup diagnostics if GCC/Clang or the native debugging engine is missing.

The extension must not automatically install or execute an untrusted debugger or compiler. Tool installation remains an explicit user action.

## Runtime safety and correctness

- Debugging is not a security sandbox; the program still runs with the user's normal permissions.
- Use argument-vector process APIs and validate executable paths.
- Do not change runtime semantics to make a variable appear inspectable.
- Keep ordinary builds independent from debugger dependencies.
- Preserve program arguments, environment variables, working directory, stdin/stdout, exit status, and termination behavior.
- Separate compiler errors from debugger startup errors and target-program failures.

## Acceptance tests

Use small YLang programs with known output and state to verify:
- Breakpoint binding to a specific source line.
- Continue, step-over, step-into, and step-out behavior.
- Correct source display for a function in an imported module.
- Call stack names and lines for nested YLang functions.
- Values of supported local integers, floats, booleans, and strings at a breakpoint.
- Handling of optimized-out/unavailable values without fabricated output.
- Missing debugger, invalid source, build failure, early exit, and user-requested termination.
- Paths with spaces and non-default working directories.
- Existing release and sanitizer tests remain passing.

## Platform support

Document debugger-engine requirements per platform. The initial support claim must match what CI and local integration tests actually exercise; a successful Linux prototype is not evidence of Windows or macOS parity.
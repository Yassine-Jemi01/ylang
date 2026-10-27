# YLang Debugger

**Status: experimental 2.2 development feature.** The CLI can create a debug build and launch the system-native debugger. This is not yet a complete YLang Debug Adapter Protocol implementation, and it does not claim full debugger support on Windows or across both code-generation backends.

## Requirements

- Linux: GCC or Clang and GDB.
- macOS: Clang and LLDB from Xcode Command Line Tools.
- Windows: the interactive `ylang debug` command is not implemented yet.

## Start a debug session

From the repository root:

```sh
make
./build/ylang debug examples/hello.yl -o build/hello-debug
```

The command performs the normal semantic checks, generates C with YLang source-line directives, compiles with `-O0`, full debug symbols, and frame pointers, then opens GDB (Linux) or LLDB (macOS). The ordinary `ylang build` optimization settings are unchanged.

## GDB commands

At the GDB prompt, set a breakpoint in the YLang source, then run the program:

```text
break examples/hello.yl:3
run
next
backtrace
info locals
continue
quit
```

Use a path matching the path shown in the build command. Imported-module statements carry their own source path when the compiler can identify the originating module.

## LLDB commands

```text
breakpoint set --file hello.yl --line 3
run
next
bt
frame variable
continue
quit
```

Adjust the source filename and line for the program being debugged.

## Current limitations

- The native debugger is operating on the compiler's generated C program with YLang source line markers. This improves source locations and breakpoints, but it is not the same as a dedicated YLang debug adapter.
- Locals may use generated C names; scope and optimized-out values may not always resemble a higher-level language debugger.
- The generated program uses the C backend. The NASM backend has no matching source-level debug contract.
- The VS Code command starts an interactive debugger in the integrated terminal. Breakpoint gutter integration, a dedicated Variables/Call Stack UI, and Debug Adapter Protocol support are future work.
- The Windows interactive debugger path is not implemented. Do not infer macOS/Windows parity from Linux test results.
- Debugging executes the target with the current user's privileges; it is not a sandbox.

Report failures with the platform, GDB/LLDB version, exact command, a small `.yl` reproducer, and the output.

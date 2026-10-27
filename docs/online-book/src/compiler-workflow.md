# The Compiler Workflow

The normal YLang workflow separates source checking from native compilation. A useful cycle is **edit, check, build, run, and test**. Keeping these stages distinct makes failures easier to locate: a source error is different from a missing C compiler, a linker failure, or a runtime problem.

## 1. Check before building

\`\`\`sh
./build/ylang check hello.yl
\`\`\`

Use \`check\` while editing. It asks the YLang compiler to validate the source and reports issues such as syntax, name, initialization, and type errors without building the final executable.

When a check fails, start with the first diagnostic. Record the file, line, column, error code, and complete message. Fix one cause at a time and run the check again; changing several unrelated things at once makes it harder to tell which change mattered.

A successful check means the source passed this compiler stage. It does not prove that a native compiler or linker will succeed, that every backend supports the same feature set, or that the program's runtime behavior is correct.

## 2. Build a native executable

\`\`\`sh
./build/ylang build hello.yl -o hello
\`\`\`

The \`-o hello\` argument chooses the output path. The default backend emits C and invokes GCC or Clang. To select Clang explicitly:

\`\`\`sh
./build/ylang build hello.yl -o hello --cc clang
\`\`\`

The C compiler is part of this route: YLang does not yet have a complete general-purpose native backend independent of GCC/Clang. If the source check succeeds but the build fails, read the native compiler or linker output as well as the YLang diagnostic.

## 3. Run and verify the result

On Linux or macOS, run an executable named \`hello\` from the current directory with:

\`\`\`sh
./hello
\`\`\`

Compare the actual output with the behavior you intended. For nontrivial programs, test normal input, boundary values, and failure cases. A successful build only confirms that the compilation steps completed; it does not guarantee that the program is logically correct.

## 4. Inspect generated C

\`\`\`sh
./build/ylang emit-c hello.yl -o build/hello.c
\`\`\`

Generated C can help you understand the compilation pipeline or investigate a compiler issue. Use it as a debugging aid, not as a stable public interface unless a specific release explicitly guarantees the generated representation.

When inspecting output, ask a focused question: Was the source expression translated as expected? Is a function call missing? Did a value get converted before it was passed onward? If the generated C appears wrong, preserve the smallest YLang program that reproduces the issue and compare the result with the program's expected behavior.

## 5. Assembly and the experimental NASM path

On supported Linux x86-64 systems, the compiler can emit assembly or build a program from the restricted NASM subset:

\`\`\`sh
./build/ylang emit-asm examples/your-program.yl -o build/your-program.asm
./build/ylang build-asm examples/your-program.yl -o build/your-program
\`\`\`

NASM is experimental. It does not support every construct accepted by the C backend, and unsupported combinations should be rejected explicitly. Do not assume that a successful C-backend build means the same source will build with NASM. Read the [Backend Roadmap](reference/BACKEND-ROADMAP.md) before choosing the assembly route for a project.

If a feature works with one backend but not the other, reduce the source to a minimal example and record which backend and compiler version you used. This makes backend-parity issues easier to reproduce.

## 6. Use the test suite

When working on the compiler itself, run the relevant tests after each change. The repository's main regression target is:

\`\`\`sh
make test
\`\`\`

For changes that affect code generation, backend parity, parsing, or runtime behavior, run the focused test as well as the broad suite when one is available. A test run is only a pass if it completes and reports success; a command that stopped early or skipped tests should not be described as a full pass.

## 7. Understand the failure stage

| Symptom | Likely stage to inspect |
| --- | --- |
| YLang syntax or type diagnostic | Source checking |
| C compiler reports an error | Generated C or native compilation |
| Undefined reference or missing library | Linking and dependencies |
| Executable starts but gives wrong output | Runtime logic or code generation |
| C backend succeeds but NASM rejects the source | Experimental backend support boundary |
| GUI builds but no window appears | Runtime environment or graphical session |

These are starting points, not guaranteed diagnoses. Read the full error and reproduce the failure before deciding what caused it.

## Summary

Use \`check\` for fast source feedback, \`build\` for the default native executable, \`emit-c\` to inspect the C translation, and the experimental assembly commands only when their limitations fit your project. For deeper implementation details, continue to [Compiler Architecture](reference/architecture.md).
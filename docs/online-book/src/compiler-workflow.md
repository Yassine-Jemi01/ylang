# The Compiler Workflow

The everyday workflow has three steps: check, build, and run.

## Check before building

```sh
./build/ylang check hello.yl
```

Use `check` while editing. It reports syntax, name, initialization, and type errors without compiling the source to a final executable.

## Build an executable

```sh
./build/ylang build hello.yl -o hello
```

The default backend emits C and calls GCC or Clang. Use `--cc clang` to select Clang explicitly:

```sh
./build/ylang build hello.yl -o hello --cc clang
```

The C compiler is part of this route: YLang does not yet have a complete general-purpose native backend independent of GCC/Clang.

## Inspect generated code

```sh
./build/ylang emit-c hello.yl -o build/hello.c
```

Generated code can help you understand the compilation pipeline or investigate a compiler issue. The [Compiler Architecture](reference/architecture.md) chapter explains the major stages.

## Experimental NASM path

On supported Linux x86-64 systems, the compiler can emit NASM assembly or build a program from the restricted NASM subset:

```sh
./build/ylang emit-asm examples/your-program.yl -o build/your-program.asm
./build/ylang build-asm examples/your-program.yl -o build/your-program
```

NASM is experimental. It does not support every construct accepted by the C backend, and unsupported combinations should be rejected explicitly. See the [Backend Roadmap](reference/BACKEND-ROADMAP.md) before choosing it for a project.

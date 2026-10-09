#include "ylang/compiler.h"

#include <stdio.h>
#include <string.h>

#ifndef YLANG_VERSION
#define YLANG_VERSION "1.0.0"
#endif

static void usage(FILE *stream)
{
    fputs(
        "YLang v" YLANG_VERSION " — stable compiled language toolchain\n\n"
        "Usage:\n"
        "  ylang check <file.yl>\n"
        "  ylang build <file.yl> [-o executable] [--cc gcc|clang]\n"
        "  ylang emit-c <file.yl> [-o generated.c]\n"
        "  ylang fix <file.yl> -o fixed.yl\n"
        "  ylang --version\n\n"
        "Commands:\n"
        "  check   Parse the source and validate types without producing a binary.\n"
        "  build   Generate C and invoke GCC or Clang to create a native executable.\n"
        "  emit-c  Write the generated C source for inspection.\n"
        "  fix     Apply high-confidence safe fixes to a new source file.\n",
        stream);
}

int main(int argc, char **argv)
{
    if (argc == 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)) {
        puts("YLang compiler " YLANG_VERSION " (C backend: GCC/Clang)");
        return 0;
    }
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout);
        return 0;
    }
    if (argc < 3) {
        usage(stderr);
        return 64;
    }
    const char *command = argv[1];
    if (strcmp(command, "check") != 0 && strcmp(command, "build") != 0 &&
        strcmp(command, "emit-c") != 0 && strcmp(command, "fix") != 0) {
        fprintf(stderr, "ylang: unknown command '%s'\n\n", command);
        usage(stderr);
        return 64;
    }
    const char *input = argv[2];
    const char *output = NULL;
    const char *cc = "gcc";
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fputs("ylang: -o needs an output path\n", stderr);
                return 64;
            }
            output = argv[++i];
        } else if (strcmp(argv[i], "--cc") == 0) {
            if (i + 1 >= argc) {
                fputs("ylang: --cc needs gcc or clang\n", stderr);
                return 64;
            }
            cc = argv[++i];
            if (strcmp(cc, "gcc") != 0 && strcmp(cc, "clang") != 0) {
                fputs("ylang: supported native compilers are gcc and clang\n", stderr);
                return 64;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout);
            return 0;
        } else {
            fprintf(stderr, "ylang: unexpected argument '%s'\n", argv[i]);
            usage(stderr);
            return 64;
        }
    }
    if (strcmp(command, "check") == 0 && output) {
        fputs("ylang: check does not accept -o\n", stderr);
        return 64;
    }
    if (strcmp(command, "fix") == 0 && !output) {
        fputs("ylang: fix requires -o <fixed.yl>; the original file is never changed implicitly\n", stderr);
        return 64;
    }
    return ylang_run(command, input, output, cc);
}

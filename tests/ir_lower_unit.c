#include "internal.h"
#include "ylang/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "IR lowering test failed: %s\n", message);
    return 1;
}

int main(void)
{
    const char *source =
        "fn add(int left, int right) -> int {\n"
        "    return left + right;\n"
        "}\n"
        "fn choose(bool condition, int left, int right) -> int {\n"
        "    let mut result: int = left;\n"
        "    if (condition) {\n"
        "        result = right;\n"
        "    } else {\n"
        "        result = result + 1;\n"
        "    }\n"
        "    return result;\n"
        "}\n"
        "fn sum_to(int limit) -> int {\n"
        "    let mut index: int = 0;\n"
        "    let mut total: int = 0;\n"
        "    while (index < limit) {\n"
        "        total += index;\n"
        "        index += 1;\n"
        "    }\n"
        "    return total;\n"
        "}\n"
        "fn main() -> int {\n"
        "    return choose(true, add(2, 3), sum_to(5));\n"
        "}\n";

    Compiler compiler;
    memset(&compiler, 0, sizeof(compiler));
    compiler.filename = "ir-lowering-unit.yl";
    compiler.source = (char *)source;
    compiler.source_length = strlen(source);
    Program *program = parse_program(&compiler);
    compiler.program = program;
    if (!program || compiler.had_error || !check_program(&compiler)) {
        arena_destroy(&compiler.arena);
        return fail("source must pass parsing and semantic checking before lowering");
    }

    YLIRModule module;
    if (!ylang_ir_lower_program(program, &module, stderr)) {
        arena_destroy(&compiler.arena);
        return fail("scalar functions, branch PHIs, loop PHIs, and calls must lower");
    }
    if (module.function_count != 4U ||
        module.functions[0]->block_count != 1U ||
        module.functions[1]->block_count < 4U ||
        module.functions[2]->block_count < 4U ||
        !ylang_ir_verify(&module, stderr)) {
        ylang_ir_module_destroy(&module);
        arena_destroy(&compiler.arena);
        return fail("lowered IR shape and SSA dominance must verify");
    }
    ylang_ir_module_destroy(&module);
    arena_destroy(&compiler.arena);
    puts("YLang AST-to-IR lowering tests passed");
    return EXIT_SUCCESS;
}

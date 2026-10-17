#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_parse(const char *source, bool expect_error,
                       size_t expected_functions)
{
    Compiler compiler = {0};
    size_t length = strlen(source);
    compiler.source = malloc(length + 1);
    if (!compiler.source) return 1;
    memcpy(compiler.source, source, length + 1);
    compiler.source_length = length;
    compiler.filename = "parser-unit.yl";

    Program *program = parse_program(&compiler);
    bool correct = program != NULL &&
                   compiler.had_error == expect_error &&
                   (!program || program->function_count == expected_functions);
    if (!correct) {
        fprintf(stderr,
                "parser unit test failed: expected error=%d, functions=%zu; got error=%d, functions=%zu\n",
                (int)expect_error, expected_functions, (int)compiler.had_error,
                program ? program->function_count : 0);
    }

    arena_destroy(&compiler.arena);
    free(compiler.source);
    return correct ? 0 : 1;
}

int main(void)
{
    const char *valid =
        "struct Point { int x; int y; }\n"
        "enum Direction { North, East, South, West }\n"
        "function main() -> int { let int total = 2 + 3; return total; }\n";
    const char *invalid =
        "function main() -> int { let int = 3; return 0; }\n";

    if (check_parse(valid, false, 1) != 0) return 1;
    if (check_parse(invalid, true, 1) != 0) return 1;
    puts("parser unit tests passed");
    return 0;
}

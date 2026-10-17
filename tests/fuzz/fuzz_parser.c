#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 64 * 1024) return 0;
    char *source = malloc(size + 1);
    if (!source) return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    Compiler compiler = {0};
    compiler.source = source;
    compiler.source_length = size;
    compiler.filename = "<libFuzzer>";
    (void)parse_program(&compiler);
    arena_destroy(&compiler.arena);
    free(source);
    return 0;
}

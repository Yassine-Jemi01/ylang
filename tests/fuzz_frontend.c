#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Optional libFuzzer entry point for malformed source input.
 * Compile with: make fuzz-frontend
 * Run with:     ./build/fuzz-frontend -runs=1000 tests/fuzz-corpus
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* Avoid spending the fuzz budget on huge parser inputs. */
    if (size > 65536U) return 0;

    char *source = malloc(size + 1U);
    if (!source) return 0;
    if (size != 0) memcpy(source, data, size);
    source[size] = '\0';

    Compiler compiler = {0};
    compiler.filename = "<fuzz-input>";
    compiler.source = source;
    compiler.source_length = size;
    (void)parse_program(&compiler);

    arena_destroy(&compiler.arena);
    free(source);
    return 0;
}

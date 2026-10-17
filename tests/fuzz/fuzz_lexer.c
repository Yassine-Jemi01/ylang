#include "ylang/lexer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 1024 * 1024) return 0;
    char *source = malloc(size + 1);
    if (!source) return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    Lexer lexer = lexer_init(source);
    size_t budget = size * 2 + 8;
    while (budget-- > 0) {
        Token token = lexer_next(&lexer);
        if (token.type == TOKEN_EOF || token.type == TOKEN_ERROR) break;
    }
    free(source);
    return 0;
}

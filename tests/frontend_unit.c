#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "frontend unit test failed: %s\n", message);
    return 1;
}

static int test_lexer(void)
{
    const char *source = "let count = 12 + 3;\n// ignored comment\ncount++ += count -> count == 3\n";
    const TokenType expected[] = {
        TOKEN_LET, TOKEN_IDENTIFIER, TOKEN_EQUAL, TOKEN_NUMBER,
        TOKEN_PLUS, TOKEN_NUMBER, TOKEN_SEMICOLON, TOKEN_NEWLINE,
        TOKEN_NEWLINE, TOKEN_IDENTIFIER, TOKEN_PLUS_PLUS, TOKEN_PLUS_EQUAL,
        TOKEN_IDENTIFIER, TOKEN_ARROW, TOKEN_IDENTIFIER, TOKEN_EQUAL_EQUAL,
        TOKEN_NUMBER, TOKEN_NEWLINE, TOKEN_EOF
    };
    Lexer lexer = lexer_init(source);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        Token token = lexer_next(&lexer);
        if (token.type != expected[i]) {
            fprintf(stderr, "lexer token %zu: expected %s, got %s at %zu:%zu\n",
                    i, token_type_name(expected[i]), token_type_name(token.type),
                    token.line, token.column);
            return 1;
        }
    }

    Lexer bad_comment = lexer_init("/* never closed");
    Token error = lexer_next(&bad_comment);
    if (error.type != TOKEN_ERROR || !error.message ||
        strstr(error.message, "Unterminated block comment") == NULL) {
        return fail("unterminated block comment must return a lexer error token");
    }
    return 0;
}

static int test_parser(void)
{
    const char *source =
        "let int base = 2;\n"
        "enum Direction { North, East }\n"
        "struct Point { int x; int y; }\n"
        "function main() -> int {\n"
        "    let Point p = Point(base, 3);\n"
        "    return p.x;\n"
        "}\n";
    Compiler compiler = {0};
    compiler.filename = "frontend-unit.yl";
    compiler.source = (char *)source;
    compiler.source_length = strlen(source);

    Program *program = parse_program(&compiler);
    const int failed = compiler.had_error || !program ||
                       program->global_count != 1 ||
                       program->function_count != 1 ||
                       compiler.enum_count != 1 ||
                       compiler.struct_count != 1;
    arena_destroy(&compiler.arena);
    if (failed) return fail("valid global, enum, struct, and function declarations should parse");
    return 0;
}

int main(void)
{
    if (test_lexer() != 0) return EXIT_FAILURE;
    if (test_parser() != 0) return EXIT_FAILURE;
    puts("Lexer/parser unit tests passed.");
    return EXIT_SUCCESS;
}

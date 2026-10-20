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

static int test_arena(void)
{
    Arena arena = {0};
    int *items = NULL;
    for (int i = 0; i < 128; i++) {
        int item = i * 3;
        items = arena_vector_append(&arena, items, (size_t)i,
                                    sizeof(*items), &item);
    }
    for (int i = 0; i < 128; i++) {
        if (items[i] != i * 3) {
            arena_destroy(&arena);
            return fail("arena vector growth must preserve existing items");
        }
    }

    char *copy = arena_strndup(&arena, "YLang compiler", 5);
    if (strcmp(copy, "YLang") != 0) {
        arena_destroy(&arena);
        return fail("arena_strndup must copy exactly the requested bytes");
    }

    unsigned char *zeroed = arena_alloc(&arena, 4);
    for (size_t i = 0; i < 4; i++) {
        if (zeroed[i] != 0) {
            arena_destroy(&arena);
            return fail("arena allocations must be zero-initialized");
        }
    }

    void *aligned = arena_alloc(&arena, sizeof(max_align_t));
    if ((uintptr_t)aligned % _Alignof(max_align_t) != 0) {
        arena_destroy(&arena);
        return fail("arena allocations must meet max_align_t alignment");
    }

    arena_destroy(&arena);
    return 0;
}

static int test_string_builder(void)
{
    StringBuilder sb;
    sb_init(&sb);
    sb_append(&sb, "YLang");
    sb_append_n(&sb, " compiler", strlen(" compiler"));
    sb_appendf(&sb, " %d", 2);

    const char *expected = "YLang compiler 2";
    const int failed = !sb.data || sb.length != strlen(expected) ||
                       strcmp(sb.data, expected) != 0;
    sb_destroy(&sb);
    if (failed) return fail("string builder append operations must preserve contents and length");
    if (sb.data != NULL || sb.length != 0 || sb.capacity != 0) {
        return fail("string builder destroy must reset its state");
    }
    return 0;
}

static int test_diagnostic_accounting(void)
{
    Compiler compiler = {0};
    compiler.quiet_diagnostics = true;
    Token token = {
        .type = TOKEN_EOF,
        .start = "",
        .length = 0,
        .line = 1,
        .column = 1,
        .message = NULL
    };

    diagnostic(&compiler, token, "error", "E9000", "test error", NULL);
    if (!compiler.had_error || compiler.error_count != 1 ||
        compiler.warning_count != 0) {
        return fail("error diagnostics must update compiler error accounting");
    }

    diagnostic(&compiler, token, "warning", "W9000", "test warning", NULL);
    if (compiler.error_count != 1 || compiler.warning_count != 1) {
        return fail("warning diagnostics must update warning accounting separately");
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
    if (test_arena() != 0) return EXIT_FAILURE;
    if (test_string_builder() != 0) return EXIT_FAILURE;
    if (test_diagnostic_accounting() != 0) return EXIT_FAILURE;
    if (test_parser() != 0) return EXIT_FAILURE;
    puts("Lexer/parser unit tests passed.");
    return EXIT_SUCCESS;
}

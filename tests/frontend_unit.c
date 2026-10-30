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

    Lexer modern_bindings = lexer_init(
        "let mut count: int = 0; let name: string = \"YLang\"; value?");
    const TokenType binding_tokens[] = {
        TOKEN_LET, TOKEN_MUT, TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER,
        TOKEN_EQUAL, TOKEN_NUMBER, TOKEN_SEMICOLON,
        TOKEN_LET, TOKEN_IDENTIFIER, TOKEN_COLON, TOKEN_IDENTIFIER,
        TOKEN_EQUAL, TOKEN_STRING, TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER, TOKEN_QUESTION, TOKEN_EOF
    };
    for (size_t i = 0; i < sizeof(binding_tokens) / sizeof(binding_tokens[0]); i++) {
        Token token = lexer_next(&modern_bindings);
        if (token.type != binding_tokens[i]) {
            fprintf(stderr, "modern binding token %zu: expected %s, got %s\n",
                    i, token_type_name(binding_tokens[i]), token_type_name(token.type));
            return 1;
        }
    }

    Lexer function_keywords = lexer_init("fn add() {} function old() {}");
    const TokenType function_tokens[] = {
        TOKEN_FN, TOKEN_IDENTIFIER, TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
        TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
        TOKEN_FUNCTION, TOKEN_IDENTIFIER, TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
        TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE, TOKEN_EOF
    };
    for (size_t i = 0; i < sizeof(function_tokens) / sizeof(function_tokens[0]); i++) {
        Token token = lexer_next(&function_keywords);
        if (token.type != function_tokens[i]) {
            fprintf(stderr, "function keyword token %zu: expected %s, got %s\n",
                    i, token_type_name(function_tokens[i]), token_type_name(token.type));
            return 1;
        }
    }

    Lexer hex_character = lexer_init("'\\x42'");
    Token character = lexer_next(&hex_character);
    if (character.type != TOKEN_CHAR || character.length != 6U) {
        return fail("lexer must consume two hexadecimal digits in a character escape");
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

static int test_escape_decoding(void)
{
    unsigned char value = 0;
    size_t consumed = 0;
    if (!decode_escape_sequence("x4142", 5U, &value, &consumed) ||
        value != 0x41U || consumed != 3U) {
        return fail("hex escapes must consume exactly two digits");
    }
    if (decode_escape_sequence("q", 1U, &value, &consumed)) {
        return fail("unknown escape sequences must be reported to the caller");
    }

    const char *literal = "\"A\\x4142\\q\\0Z\"";
    Token string_token = {
        .type = TOKEN_STRING,
        .start = literal,
        .length = strlen(literal),
        .line = 1,
        .column = 1,
        .message = NULL
    };
    unsigned char *bytes = NULL;
    size_t length = 0;
    const unsigned char expected[] = { 'A', 'A', '4', '2', 'q', 0, 'Z' };
    if (!decode_quoted_literal(string_token, &bytes, &length)) {
        return fail("quoted strings must decode successfully");
    }
    const int bad_string = length != sizeof(expected) ||
                           memcmp(bytes, expected, sizeof(expected)) != 0;
    free(bytes);
    if (bad_string) {
        return fail("quoted string decoding must preserve exact bytes and embedded NULs");
    }

    const char *character = "'\\n'";
    Token char_token = {
        .type = TOKEN_CHAR,
        .start = character,
        .length = strlen(character),
        .line = 1,
        .column = 1,
        .message = NULL
    };
    bytes = NULL;
    length = 0;
    if (!decode_quoted_literal(char_token, &bytes, &length)) {
        return fail("quoted character escapes must decode successfully");
    }
    const int bad_char = length != 1U || bytes[0] != (unsigned char)'\n';
    free(bytes);
    if (bad_char) {
        return fail("character escape decoding must match string escape decoding");
    }
    return 0;
}

static int test_parser(void)
{
    const char *source =
        "let mut base: int = 2;\n"
        "let answer: int = 42;\n"
        "let mut offset: int = 1;\n"
        "enum Direction { North, East }\n"
        "struct Point { int x; int y; }\n"
        "fn main() -> int {\n"
        "    let p: Point = Point(base, 3);\n"
        "    let name: string = \"YLang\";\n"
        "    let mut count: int = 0;\n"
        "    count = count + offset;\n"
        "    return p.x + count;\n"
        "}\n"
        "function legacy() -> int { return 0; }\n";
    Compiler compiler = {0};
    compiler.filename = "frontend-unit.yl";
    compiler.source = (char *)source;
    compiler.source_length = strlen(source);

    Program *program = parse_program(&compiler);
    const int failed = compiler.had_error || !program ||
                       program->global_count != 3 ||
                       program->function_count != 2 ||
                       compiler.warning_count != 1 ||
                       compiler.enum_count != 1 ||
                       compiler.struct_count != 1;
    arena_destroy(&compiler.arena);
    if (failed) return fail("fn parses cleanly and legacy function parses with one W2302 warning");
    return 0;
}

int main(void)
{
    if (test_lexer() != 0) return EXIT_FAILURE;
    if (test_arena() != 0) return EXIT_FAILURE;
    if (test_string_builder() != 0) return EXIT_FAILURE;
    if (test_diagnostic_accounting() != 0) return EXIT_FAILURE;
    if (test_escape_decoding() != 0) return EXIT_FAILURE;
    if (test_parser() != 0) return EXIT_FAILURE;
    puts("Frontend unit tests passed.");
    return EXIT_SUCCESS;
}

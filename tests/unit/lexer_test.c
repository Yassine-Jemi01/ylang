#include "ylang/lexer.h"

#include <stdio.h>
#include <string.h>

static int expect_token(Lexer *lexer, TokenType expected, const char *spelling)
{
    Token token = lexer_next(lexer);
    size_t length = strlen(spelling);
    if (token.type != expected ||
        (spelling[0] != '\0' &&
         (token.length != length || memcmp(token.start, spelling, length) != 0))) {
        fprintf(stderr, "lexer unit test: expected %s '%s', got %s at %zu:%zu\n",
                token_type_name(expected), spelling, token_type_name(token.type),
                token.line, token.column);
        return 1;
    }
    return 0;
}

int main(void)
{
    const char *source =
        "function main() -> int { let value = 0x2a; print(\"hi\"); } // tail\n"
        "/* block */ return value;";
    Lexer lexer = lexer_init(source);
    const struct {
        TokenType type;
        const char *spelling;
    } expected[] = {
        {TOKEN_FUNCTION, "function"}, {TOKEN_IDENTIFIER, "main"},
        {TOKEN_LEFT_PAREN, "("}, {TOKEN_RIGHT_PAREN, ")"},
        {TOKEN_ARROW, "->"}, {TOKEN_IDENTIFIER, "int"},
        {TOKEN_LEFT_BRACE, "{"}, {TOKEN_LET, "let"},
        {TOKEN_IDENTIFIER, "value"}, {TOKEN_EQUAL, "="},
        {TOKEN_NUMBER, "0x2a"}, {TOKEN_SEMICOLON, ";"},
        {TOKEN_PRINT, "print"}, {TOKEN_LEFT_PAREN, "("},
        {TOKEN_STRING, "\"hi\""}, {TOKEN_RIGHT_PAREN, ")"},
        {TOKEN_SEMICOLON, ";"}, {TOKEN_RIGHT_BRACE, "}"},
        {TOKEN_NEWLINE, "\n"}, {TOKEN_RETURN, "return"},
        {TOKEN_IDENTIFIER, "value"}, {TOKEN_SEMICOLON, ";"},
        {TOKEN_EOF, ""}
    };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        if (expect_token(&lexer, expected[i].type, expected[i].spelling) != 0) {
            return 1;
        }
    }

    Lexer malformed = lexer_init("0x;");
    Token token = lexer_next(&malformed);
    if (token.type != TOKEN_ERROR || token.message == NULL) {
        fputs("lexer unit test: malformed hexadecimal literal was not rejected\n", stderr);
        return 1;
    }

    Lexer unterminated_comment = lexer_init("/* missing close");
    token = lexer_next(&unterminated_comment);
    if (token.type != TOKEN_ERROR || token.message == NULL) {
        fputs("lexer unit test: unterminated block comment was not rejected\n", stderr);
        return 1;
    }

    puts("lexer unit tests passed");
    return 0;
}

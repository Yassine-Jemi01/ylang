#ifndef YLANG_LEXER_H
#define YLANG_LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_FSTRING,
    TOKEN_NEWLINE,

    TOKEN_LET,
    TOKEN_CONST,
    TOKEN_MUT,
    TOKEN_PRINT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_FUNCTION,
    TOKEN_LOOP,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_CLASS,

    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_DOT,
    TOKEN_AMPERSAND,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ARROW,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    TOKEN_SEMICOLON,
    TOKEN_COMMA
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
    const char *message;
} Token;

typedef struct {
    const char *source;
    size_t start;
    size_t current;
    size_t line;
    size_t column;
    size_t token_line;
    size_t token_column;
} Lexer;

Lexer lexer_init(const char *source);
Token lexer_next(Lexer *lexer);
const char *token_type_name(TokenType type);

#endif

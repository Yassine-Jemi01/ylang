#include "ylang/lexer.h"

#include <stdbool.h>
#include <string.h>

static char peek(const Lexer *lexer)
{
    return lexer->source[lexer->current];
}

static char peek_next(const Lexer *lexer)
{
    if (peek(lexer) == '\0') {
        return '\0';
    }

    return lexer->source[lexer->current + 1];
}

static char advance_char(Lexer *lexer)
{
    char c = peek(lexer);

    if (c == '\0') {
        return c;
    }

    lexer->current++;

    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }

    return c;
}

static bool match_char(Lexer *lexer, char expected)
{
    if (peek(lexer) != expected) {
        return false;
    }

    advance_char(lexer);
    return true;
}

static Token make_token(const Lexer *lexer, TokenType type)
{
    Token token = {
        .type = type,
        .start = lexer->source + lexer->start,
        .length = lexer->current - lexer->start,
        .line = lexer->token_line,
        .column = lexer->token_column,
        .message = NULL
    };

    return token;
}

static Token error_token(const Lexer *lexer, const char *message)
{
    Token token = make_token(lexer, TOKEN_ERROR);
    token.message = message;
    return token;
}

Lexer lexer_init(const char *source)
{
    Lexer lexer = {
        .source = source != NULL ? source : "",
        .start = 0,
        .current = 0,
        .line = 1,
        .column = 1,
        .token_line = 1,
        .token_column = 1
    };

    return lexer;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void skip_spaces_and_comments(Lexer *lexer)
{
    for (;;) {
        char c = peek(lexer);

        if (c == ' ' || c == '\t' || c == '\r' ||
            c == '\f' || c == '\v') {
            advance_char(lexer);
        } else if (c == '/' && peek_next(lexer) == '/') {
            while (peek(lexer) != '\n' && peek(lexer) != '\0') {
                advance_char(lexer);
            }
        } else {
            return;
        }
    }
}

static bool is_word(const Lexer *lexer, const char *word)
{
    size_t length = strlen(word);

    return lexer->current - lexer->start == length &&
           memcmp(lexer->source + lexer->start, word, length) == 0;
}

static TokenType identifier_type(const Lexer *lexer)
{
    if (is_word(lexer, "let")) return TOKEN_LET;
    if (is_word(lexer, "const")) return TOKEN_CONST;
    if (is_word(lexer, "mut")) return TOKEN_MUT;
    if (is_word(lexer, "print")) return TOKEN_PRINT;
    if (is_word(lexer, "if")) return TOKEN_IF;
    if (is_word(lexer, "else")) return TOKEN_ELSE;
    if (is_word(lexer, "true")) return TOKEN_TRUE;
    if (is_word(lexer, "false")) return TOKEN_FALSE;
    if (is_word(lexer, "function")) return TOKEN_FUNCTION;
    if (is_word(lexer, "loop")) return TOKEN_LOOP;
    if (is_word(lexer, "break")) return TOKEN_BREAK;
    if (is_word(lexer, "continue")) return TOKEN_CONTINUE;
    if (is_word(lexer, "return")) return TOKEN_RETURN;
    if (is_word(lexer, "and")) return TOKEN_AND;
    if (is_word(lexer, "or")) return TOKEN_OR;
    if (is_word(lexer, "not")) return TOKEN_NOT;
    if (is_word(lexer, "try")) return TOKEN_TRY;
    if (is_word(lexer, "catch")) return TOKEN_CATCH;
    if (is_word(lexer, "class")) return TOKEN_CLASS;

    return TOKEN_IDENTIFIER;
}

static Token scan_identifier(Lexer *lexer)
{
    while (is_alpha(peek(lexer)) || is_digit(peek(lexer))) {
        advance_char(lexer);
    }

    return make_token(lexer, identifier_type(lexer));
}

static Token scan_number(Lexer *lexer)
{
    while (is_digit(peek(lexer))) {
        advance_char(lexer);
    }

    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        advance_char(lexer);
        while (is_digit(peek(lexer))) {
            advance_char(lexer);
        }
    }

    return make_token(lexer, TOKEN_NUMBER);
}

static Token scan_string(Lexer *lexer, TokenType type)
{
    while (peek(lexer) != '"' && peek(lexer) != '\0') {
        if (peek(lexer) == '\n') {
            return error_token(lexer, "String literals cannot contain a newline.");
        }

        if (peek(lexer) == '\\') {
            advance_char(lexer);

            if (peek(lexer) == '\0' || peek(lexer) == '\n') {
                break;
            }

            advance_char(lexer);
        } else {
            advance_char(lexer);
        }
    }

    if (peek(lexer) != '"') {
        return error_token(lexer, "Unterminated string literal.");
    }

    advance_char(lexer);
    return make_token(lexer, type);
}

Token lexer_next(Lexer *lexer)
{
    skip_spaces_and_comments(lexer);

    lexer->start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->column;

    char c = advance_char(lexer);

    if (c == '\0') return make_token(lexer, TOKEN_EOF);
    if (c == '\n') return make_token(lexer, TOKEN_NEWLINE);

    /* An f-string is one token for now; interpolation is parsed later. */
    if (c == 'f' && peek(lexer) == '"') {
        advance_char(lexer);
        return scan_string(lexer, TOKEN_FSTRING);
    }

    if (is_alpha(c)) return scan_identifier(lexer);
    if (is_digit(c)) return scan_number(lexer);

    switch (c) {
        case '(' : return make_token(lexer, TOKEN_LEFT_PAREN);
        case ')' : return make_token(lexer, TOKEN_RIGHT_PAREN);
        case '{' : return make_token(lexer, TOKEN_LEFT_BRACE);
        case '}' : return make_token(lexer, TOKEN_RIGHT_BRACE);
        case '[' : return make_token(lexer, TOKEN_LEFT_BRACKET);
        case ']' : return make_token(lexer, TOKEN_RIGHT_BRACKET);
        case '.' : return make_token(lexer, TOKEN_DOT);
        case '&' : return make_token(lexer, TOKEN_AMPERSAND);
        case '+' : return make_token(lexer, TOKEN_PLUS);
        case '*' : return make_token(lexer, TOKEN_STAR);
        case '/' : return make_token(lexer, TOKEN_SLASH);
        case '%' : return make_token(lexer, TOKEN_PERCENT);
        case ';' : return make_token(lexer, TOKEN_SEMICOLON);
        case ',' : return make_token(lexer, TOKEN_COMMA);

        case '-':
            return make_token(lexer, match_char(lexer, '>') ? TOKEN_ARROW : TOKEN_MINUS);

        case '=':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);

        case '!':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);

        case '<':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

        case '>':
            return make_token(lexer, match_char(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

        case '"':
            return scan_string(lexer, TOKEN_STRING);

        case '\'': {
            /* YLang 1.0 char literals contain exactly one byte or one escape. */
            if (peek(lexer) == '\\') {
                advance_char(lexer);
                if (peek(lexer) != '\0' && peek(lexer) != '\n') {
                    advance_char(lexer);
                }
            } else if (peek(lexer) != '\0' && peek(lexer) != '\n' && peek(lexer) != '\'') {
                advance_char(lexer);
            }
            if (peek(lexer) != '\'') {
                return error_token(lexer, "Expected one character followed by a closing quote.");
            }
            advance_char(lexer);
            return make_token(lexer, TOKEN_CHAR);
        }

        default:
            return error_token(lexer, "Unexpected character.");
    }
}

const char *token_type_name(TokenType type)
{
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_FSTRING: return "FSTRING";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_LET: return "LET";
        case TOKEN_CONST: return "CONST";
        case TOKEN_MUT: return "MUT";
        case TOKEN_PRINT: return "PRINT";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_FUNCTION: return "FUNCTION";
        case TOKEN_LOOP: return "LOOP";
        case TOKEN_BREAK: return "BREAK";
        case TOKEN_CONTINUE: return "CONTINUE";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_TRY: return "TRY";
        case TOKEN_CATCH: return "CATCH";
        case TOKEN_CLASS: return "CLASS";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TOKEN_DOT: return "DOT";
        case TOKEN_AMPERSAND: return "AMPERSAND";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_EQUAL: return "EQUAL";
        case TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TOKEN_BANG: return "BANG";
        case TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case TOKEN_LESS: return "LESS";
        case TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_COMMA: return "COMMA";
    }

    return "UNKNOWN_TOKEN";
}

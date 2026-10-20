#include "internal.h"

#include <stdlib.h>

static bool hex_digit_value(unsigned char ch, unsigned *value)
{
    if (ch >= '0' && ch <= '9') {
        *value = (unsigned)(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        *value = (unsigned)(ch - 'a') + 10U;
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        *value = (unsigned)(ch - 'A') + 10U;
        return true;
    }
    return false;
}

/*
 * Decode the characters immediately after a backslash.
 * `consumed` is the number of source characters after the backslash.
 * On an unknown or incomplete escape, return false and leave outputs alone.
 */
bool decode_escape_sequence(const char *text, size_t available,
                            unsigned char *value, size_t *consumed)
{
    if (!text || available == 0U || !value || !consumed) return false;

    unsigned char ch = (unsigned char)text[0];
    switch (ch) {
        case 'n': *value = (unsigned char)'\n'; *consumed = 1U; return true;
        case 'r': *value = (unsigned char)'\r'; *consumed = 1U; return true;
        case 't': *value = (unsigned char)'\t'; *consumed = 1U; return true;
        case '0': *value = 0U; *consumed = 1U; return true;
        case '\\': *value = (unsigned char)'\\'; *consumed = 1U; return true;
        case '\'': *value = (unsigned char)'\''; *consumed = 1U; return true;
        case '"': *value = (unsigned char)'"'; *consumed = 1U; return true;
        case '{': *value = (unsigned char)'{'; *consumed = 1U; return true;
        case '}': *value = (unsigned char)'}'; *consumed = 1U; return true;
        case '/': *value = (unsigned char)'/'; *consumed = 1U; return true;
        case 'x': {
            if (available < 3U) return false;
            unsigned high = 0;
            unsigned low = 0;
            if (!hex_digit_value((unsigned char)text[1], &high) ||
                !hex_digit_value((unsigned char)text[2], &low)) {
                return false;
            }
            *value = (unsigned char)((high << 4U) | low);
            *consumed = 3U;
            return true;
        }
        default:
            return false;
    }
}

/*
 * Decode a quoted YLang string or character token into bytes.
 * Recognized escapes share one implementation across the C and NASM backends.
 * For compatibility with the existing NASM backend, unknown escapes drop the
 * backslash and preserve the escaped character (for example, \\q becomes q).
 */
bool decode_quoted_literal(Token token, unsigned char **bytes, size_t *length)
{
    if (!bytes || !length) return false;
    *bytes = NULL;
    *length = 0U;

    if (token.length < 2U || !token.start) return false;
    const char quote = token.start[0];
    if ((quote != '"' && quote != '\'') ||
        token.start[token.length - 1U] != quote) {
        return false;
    }

    unsigned char *result = malloc(token.length);
    if (!result) return false;

    const size_t end = token.length - 1U;
    size_t out = 0U;
    for (size_t i = 1U; i < end; i++) {
        unsigned char ch = (unsigned char)token.start[i];
        if (ch == (unsigned char)'\\' && i + 1U < end) {
            unsigned char decoded = 0U;
            size_t consumed = 0U;
            if (decode_escape_sequence(token.start + i + 1U, end - i - 1U,
                                       &decoded, &consumed)) {
                ch = decoded;
                i += consumed;
            } else {
                ch = (unsigned char)token.start[++i];
            }
        }
        result[out++] = ch;
    }

    *bytes = result;
    *length = out;
    return true;
}

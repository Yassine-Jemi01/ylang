#include "internal.h"

#include <string.h>

#define YLANG_MAX_DIAGNOSTICS 100U
#define YLANG_MAX_DIAGNOSTIC_LINE_BYTES 160U

static bool utf8_continuation(unsigned char byte)
{
    return byte >= 0x80U && byte <= 0xBFU;
}

static size_t valid_utf8_sequence_length(const unsigned char *text, size_t length)
{
    if (length < 2U) return 0U;
    unsigned char first = text[0];
    unsigned char second = text[1];
    if (first >= 0xC2U && first <= 0xDFU)
        return utf8_continuation(second) ? 2U : 0U;
    if (length >= 3U && first >= 0xE0U && first <= 0xEFU) {
        unsigned char third = text[2];
        bool second_ok = first == 0xE0U ? (second >= 0xA0U && second <= 0xBFU) :
                         first == 0xEDU ? (second >= 0x80U && second <= 0x9FU) :
                         utf8_continuation(second);
        return second_ok && utf8_continuation(third) ? 3U : 0U;
    }
    if (length >= 4U && first >= 0xF0U && first <= 0xF4U) {
        unsigned char third = text[2];
        unsigned char fourth = text[3];
        bool second_ok = first == 0xF0U ? (second >= 0x90U && second <= 0xBFU) :
                         first == 0xF4U ? (second >= 0x80U && second <= 0x8FU) :
                         utf8_continuation(second);
        return second_ok && utf8_continuation(third) &&
               utf8_continuation(fourth) ? 4U : 0U;
    }
    return 0U;
}

static void print_hex_escape(FILE *out, unsigned char byte)
{
    (void)fprintf(out, "\\x%02X", (unsigned)byte);
}

static void print_sanitized_bytes(FILE *out, const unsigned char *text, size_t length)
{
    size_t i = 0U;
    while (i < length) {
        unsigned char byte = text[i];
        if (byte < 0x20U || byte == 0x7FU) {
            print_hex_escape(out, byte);
            i++;
        } else if (byte < 0x80U) {
            (void)fputc((int)byte, out);
            i++;
        } else {
            size_t sequence = valid_utf8_sequence_length(text + i, length - i);
            if (!sequence) {
                print_hex_escape(out, byte);
                i++;
            } else if (sequence == 2U && text[i] == 0xC2U &&
                       text[i + 1U] >= 0x80U && text[i + 1U] <= 0x9FU) {
                print_hex_escape(out, text[i]);
                print_hex_escape(out, text[i + 1U]);
                i += 2U;
            } else {
                (void)fwrite(text + i, 1U, sequence, out);
                i += sequence;
            }
        }
    }
}

static size_t sanitized_display_width(const unsigned char *text, size_t length)
{
    size_t width = 0U;
    size_t i = 0U;
    while (i < length) {
        unsigned char byte = text[i];
        if (byte < 0x20U || byte == 0x7FU) {
            width += 4U;
            i++;
        } else if (byte < 0x80U) {
            width++;
            i++;
        } else {
            size_t sequence = valid_utf8_sequence_length(text + i, length - i);
            if (!sequence) {
                width += 4U;
                i++;
            } else if (sequence == 2U && text[i] == 0xC2U &&
                       text[i + 1U] >= 0x80U && text[i + 1U] <= 0x9FU) {
                width += 8U;
                i += 2U;
            } else {
                width++;
                i += sequence;
            }
        }
    }
    return width;
}

static void print_sanitized_cstr(FILE *out, const char *text)
{
    if (!text) {
        fputs("(unknown)", out);
        return;
    }
    print_sanitized_bytes(out, (const unsigned char *)text, strlen(text));
}

void diagnostic(Compiler *c, Token token, const char *level,
                const char *code, const char *message,
                const char *suggestion)
{
    if (strcmp(level, "error") == 0) {
        c->had_error = true;
        c->error_count++;
    } else {
        c->warning_count++;
    }
    if (c->diagnostic_count <= YLANG_MAX_DIAGNOSTICS) c->diagnostic_count++;
    if (c->diagnostic_count > YLANG_MAX_DIAGNOSTICS) {
        if (c->diagnostic_count == YLANG_MAX_DIAGNOSTICS + 1U)
            fputs("error[E1099]: Too many diagnostics; further messages are suppressed.\n",
                  stderr);
        return;
    }
    if (c->quiet_diagnostics) return;

    size_t line = token.line ? token.line : 1U;
    size_t column = token.column ? token.column : 1U;
    const char *shown_filename = c->filename;
    const char *shown_source = c->source;
    size_t shown_source_length = c->source_length;
    bool imported_source = false;
    uintptr_t token_address = (uintptr_t)(const void *)token.start;
    for (size_t i = 0; i < c->module_count; i++) {
        Module *module = c->modules[i];
        uintptr_t base = (uintptr_t)(const void *)module->source;
        if (token_address >= base &&
            token_address - base <= module->source_length) {
            shown_filename = module->path;
            shown_source = module->source;
            shown_source_length = module->source_length;
            imported_source = !module->is_root;
            break;
        }
    }

    const char *shown_message = imported_source ?
        "Imported module contains a diagnostic; source details are redacted." :
        (message ? message : "Compiler diagnostic.");
    fputs(level, stderr);
    fputc('[', stderr);
    print_sanitized_cstr(stderr, code);
    fputs("]: ", stderr);
    print_sanitized_cstr(stderr, shown_message);
    fputc('\n', stderr);
    fputs("  --> ", stderr);
    print_sanitized_cstr(stderr, shown_filename);
    (void)fprintf(stderr, ":%zu:%zu\n", line, column);

    if (imported_source) {
        fputs("   | [source excerpt and help text omitted for imported module]\n",
              stderr);
    } else if (shown_source && shown_source_length > 0U) {
        const char *source_end = shown_source + shown_source_length;
        const char *line_start = shown_source;
        size_t current_line = 1U;
        while (line_start < source_end && current_line < line) {
            if (*line_start++ == '\n') current_line++;
        }
        const char *line_end = line_start;
        while (line_end < source_end && *line_end != '\n' && *line_end != '\r')
            line_end++;
        if (line_start < source_end && current_line == line) {
            size_t line_length = (size_t)(line_end - line_start);
            size_t token_offset = column > 0U ? column - 1U : 0U;
            if (token_offset > line_length) token_offset = line_length;
            size_t window_start = token_offset > 80U ? token_offset - 80U : 0U;
            if (line_length > YLANG_MAX_DIAGNOSTIC_LINE_BYTES &&
                window_start > line_length - YLANG_MAX_DIAGNOSTIC_LINE_BYTES)
                window_start = line_length - YLANG_MAX_DIAGNOSTIC_LINE_BYTES;
            size_t window_end = line_length - window_start >
                                YLANG_MAX_DIAGNOSTIC_LINE_BYTES ?
                                window_start + YLANG_MAX_DIAGNOSTIC_LINE_BYTES :
                                line_length;
            bool has_prefix = window_start > 0U;
            bool has_suffix = window_end < line_length;
            size_t caret_offset = token_offset > window_start ?
                                  token_offset - window_start : 0U;
            if (caret_offset > window_end - window_start)
                caret_offset = window_end - window_start;
            size_t caret_column = (has_prefix ? 3U : 0U) +
                sanitized_display_width((const unsigned char *)line_start + window_start,
                                        caret_offset);

            fputs("   |\n", stderr);
            (void)fprintf(stderr, "%3zu | ", line);
            if (has_prefix) fputs("...", stderr);
            print_sanitized_bytes(stderr,
                (const unsigned char *)line_start + window_start,
                window_end - window_start);
            if (has_suffix) fputs("...", stderr);
            fputs("\n   | ", stderr);
            for (size_t i = 0U; i < caret_column; i++) fputc(' ', stderr);
            size_t width = token.length > 0U ? token.length : 1U;
            if (width > 60U) width = 1U;
            for (size_t i = 0U; i < width; i++) fputc('^', stderr);
            fputc('\n', stderr);
        }
        if (suggestion && *suggestion) {
            fputs("   = help: ", stderr);
            print_sanitized_cstr(stderr, suggestion);
            fputc('\n', stderr);
        }
    }
    fputc('\n', stderr);
}

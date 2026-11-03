#include "internal.h"

#include <string.h>

#define YLANG_MAX_DIAGNOSTICS 100U
#define YLANG_DIAGNOSTIC_LINE_WINDOW 160U

static void print_sanitized(FILE *out, const char *text, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 0x20U || ch >= 0x7fU) {
            fprintf(out, "\\x%02X", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
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
    if (c->quiet_diagnostics) return;
    if (c->error_count > YLANG_MAX_DIAGNOSTICS) {
        if (c->error_count == YLANG_MAX_DIAGNOSTICS + 1U) {
            fputs("error: too many errors; stopping after 100 diagnostics.\\n\\n", stderr);
        }
        return;
    }

    size_t line = token.line ? token.line : 1;
    size_t column = token.column ? token.column : 1;
    const char *shown_filename = c->filename;
    const char *shown_source = c->source;
    size_t shown_source_length = c->source_length;
    uintptr_t token_address = (uintptr_t)(const void *)token.start;
    for (size_t i = 0; i < c->module_count; i++) {
        Module *module = c->modules[i];
        uintptr_t base = (uintptr_t)(const void *)module->source;
        if (token_address >= base &&
            token_address - base <= module->source_length) {
            shown_filename = module->path;
            shown_source = module->source;
            shown_source_length = module->source_length;
            break;
        }
    }

    fprintf(stderr, "%s[%s]: %s\\n", level, code, message);
    fputs("  --> ", stderr);
    if (shown_filename) print_sanitized(stderr, shown_filename, strlen(shown_filename));
    fprintf(stderr, ":%zu:%zu\\n", line, column);

    const char *line_start = shown_source;
    size_t current_line = 1;
    while (shown_source && current_line < line &&
           line_start < shown_source + shown_source_length) {
        if (*line_start++ == '\\n') current_line++;
    }
    const char *line_end = line_start;
    while (shown_source && line_end < shown_source + shown_source_length &&
           *line_end && *line_end != '\\n' && *line_end != '\\r') line_end++;
    if (shown_source && line_start < shown_source + shown_source_length &&
        *line_start && current_line == line) {
        size_t source_line_length = (size_t)(line_end - line_start);
        size_t caret_offset = column > 0U ? column - 1U : 0U;
        size_t window_start = caret_offset > YLANG_DIAGNOSTIC_LINE_WINDOW / 2U
                                  ? caret_offset - YLANG_DIAGNOSTIC_LINE_WINDOW / 2U : 0U;
        if (window_start > source_line_length) window_start = source_line_length;
        size_t window_end = source_line_length;
        if (window_end - window_start > YLANG_DIAGNOSTIC_LINE_WINDOW)
            window_end = window_start + YLANG_DIAGNOSTIC_LINE_WINDOW;

        fputs("   |\\n", stderr);
        fprintf(stderr, "%3zu | ", line);
        if (window_start > 0U) fputs("...", stderr);
        print_sanitized(stderr, line_start + window_start, window_end - window_start);
        if (window_end < source_line_length) fputs("...", stderr);
        fputs("\\n   | ", stderr);

        size_t caret_column = caret_offset >= window_start
                                  ? caret_offset - window_start : 0U;
        if (window_start > 0U) caret_column += 3U;
        if (caret_column > YLANG_DIAGNOSTIC_LINE_WINDOW + 3U)
            caret_column = YLANG_DIAGNOSTIC_LINE_WINDOW + 3U;
        for (size_t i = 0; i < caret_column; i++) fputc(' ', stderr);

        size_t width = token.length > 0U ? token.length : 1U;
        if (width > 60U) width = 1U;
        size_t visible_width = window_end - window_start;
        if (caret_offset >= window_start && caret_offset < window_end &&
            width > visible_width - (caret_offset - window_start))
            width = visible_width - (caret_offset - window_start);
        for (size_t i = 0; i < width; i++) fputc('^', stderr);
        fputc('\\n', stderr);
    }
    if (suggestion && *suggestion) {
        fprintf(stderr, "   = help: %s\\n", suggestion);
    }
    fputc('\\n', stderr);
}

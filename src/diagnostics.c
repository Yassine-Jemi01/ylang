#include "internal.h"

#include <string.h>

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
    fprintf(stderr, "%s[%s]: %s\n", level, code, message);
    fprintf(stderr, "  --> %s:%zu:%zu\n", shown_filename, line, column);

    const char *line_start = shown_source;
    size_t current_line = 1;
    while (current_line < line && *line_start) {
        if (*line_start++ == '\n') current_line++;
    }
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    if (shown_source && line_start < shown_source + shown_source_length &&
        *line_start && current_line == line) {
        fprintf(stderr, "   |\n%3zu | %.*s\n   | ", line,
                (int)(line_end - line_start), line_start);
        for (size_t i = 1; i < column; i++) fputc(' ', stderr);
        size_t width = token.length > 0 ? token.length : 1;
        if (width > 60) width = 1;
        for (size_t i = 0; i < width; i++) fputc('^', stderr);
        fputc('\n', stderr);
    }
    if (suggestion && *suggestion) {
        fprintf(stderr, "   = help: %s\n", suggestion);
    }
    fputc('\n', stderr);
}


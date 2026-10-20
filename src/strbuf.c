#include "internal.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void sb_init(StringBuilder *sb)
{
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

static void sb_reserve(StringBuilder *sb, size_t extra)
{
    if (sb->length == SIZE_MAX || extra > SIZE_MAX - sb->length - 1) {
        fputs("ylang: fatal: generated text is too large\n", stderr);
        exit(70);
    }
    size_t required = sb->length + extra + 1;
    if (required <= sb->capacity) return;
    size_t capacity = sb->capacity ? sb->capacity : 128;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    char *data = realloc(sb->data, capacity);
    if (!data) {
        fputs("ylang: fatal: out of memory\n", stderr);
        exit(70);
    }
    sb->data = data;
    sb->capacity = capacity;
}

void sb_append_n(StringBuilder *sb, const char *text, size_t length)
{
    sb_reserve(sb, length);
    memcpy(sb->data + sb->length, text, length);
    sb->length += length;
    sb->data[sb->length] = '\0';
}

void sb_append(StringBuilder *sb, const char *text)
{
    sb_append_n(sb, text, strlen(text));
}

void sb_appendf(StringBuilder *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return;
    }
    sb_reserve(sb, (size_t)needed);
    (void)vsnprintf(sb->data + sb->length, sb->capacity - sb->length,
                    format, args);
    sb->length += (size_t)needed;
    va_end(args);
}

void sb_destroy(StringBuilder *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->length = sb->capacity = 0;
}


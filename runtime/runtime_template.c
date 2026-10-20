#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

bool ylgui_api_init(void);
bool ylgui_api_create_window(const char *, int64_t, int64_t);
bool ylgui_api_poll(void);
bool ylgui_api_key_down(const char *);
int64_t ylgui_api_mouse_x(void);
int64_t ylgui_api_mouse_y(void);
bool ylgui_api_mouse_button_down(int64_t);
int64_t ylgui_api_ticks_ms(void);
bool ylgui_api_clear(int64_t, int64_t, int64_t);
bool ylgui_api_draw_rect(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
bool ylgui_api_present(void);
const char *ylgui_api_last_error(void);
void ylgui_api_close(void);
void ylgui_api_shutdown(void);

static void yl_runtime_error(const char *message) {
    fprintf(stderr, "YLang runtime error: %s\n", message);
    exit(70);
}
typedef struct YLTracked { void *ptr; struct YLTracked *next; } YLTracked;
static YLTracked *yl_tracked = NULL;
static void yl_cleanup(void) {
    while (yl_tracked) {
        YLTracked *next = yl_tracked->next;
        free(yl_tracked->ptr);
        free(yl_tracked);
        yl_tracked = next;
    }

    YLTracked *node = malloc(sizeof(*node));
    if (!node) yl_runtime_error("out of memory");
    node->ptr = ptr;
    node->next = yl_tracked;
    yl_tracked = node;
    return ptr;
(bool value) { return value ? "true" : "false"; }
static const char *yl_format(const char *format, ...);
static const char *yl_to_string_int(int64_t value) { return yl_format("%lld", (long long)value); }
static const char *yl_to_string_float(double value) { return yl_format("%.17g", value); }
static const char *yl_to_string_char(char value) { return yl_format("%c", (int)value); }
static int64_t yl_to_int(double value) {
    if (!isfinite(value) ||
        value < -9223372036854775808.0 ||
        value >= 9223372036854775808.0) {
        yl_runtime_error("to_int value is outside the signed 64-bit range");
    }
    return (int64_t)value;
}
static bool yl_string_is_int(const char *value) {
    if (!value || !*value) return false;

    errno = 0;
    char *end = NULL;
    (void)strtoll(value, &end, 10);
    return errno != ERANGE && end != value && end && *end == '\0';
}
static int64_t yl_string_parse_int(const char *value) {
    if (!yl_string_is_int(value)) {
        yl_runtime_error("string.parse_int received invalid or out-of-range integer text");
    }
    return (int64_t)strtoll(value, NULL, 10);
}
static const char *yl_format(const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);

    if (needed < 0) {
        va_end(args);
        yl_runtime_error("formatting failed");
    }

    char *buffer = malloc((size_t)needed + 1);
    if (!buffer) {
        va_end(args);
        yl_runtime_error("out of memory");
    }
    (void)vsnprintf(buffer, (size_t)needed + 1, format, args);
    va_end(args);
    return (const char *)yl_track(buffer);
(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_add_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in addition");
    }
    return result;
}
static int64_t yl_add_assign_i64(int64_t *target, int64_t value) {
    *target = yl_add_i64(*target, value);
    return *target;
}
static double yl_add_assign_f64(double *target, double value) {
    *target += value;
    return *target;
}
static int64_t yl_sub_i64(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_sub_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in subtraction");
    }
    return result;
}
static int64_t yl_mul_i64(int64_t a, int64_t b) {
    int64_t result;
    if (__builtin_mul_overflow(a, b, &result)) {
        yl_runtime_error("integer overflow in multiplication");
    }
    return result;
}
static int64_t yl_neg_i64(int64_t value) {
    if (value == INT64_MIN) yl_runtime_error("integer overflow in negation");
    return -value;
}
static int64_t yl_div_i64(int64_t a, int64_t b) {
    if (b == 0) yl_runtime_error("integer division by zero");
    if (a == INT64_MIN && b == -1) {
        yl_runtime_error("integer overflow in division");
    }
    return a / b;
}
static int64_t yl_mod_i64(int64_t a, int64_t b) {
    if (b == 0) yl_runtime_error("integer modulo by zero");
    if (a == INT64_MIN && b == -1) return 0;
    return a % b;
}
static double yl_div_f64(double a, double b) {
    if (b == 0.0) yl_runtime_error("floating-point division by zero");
    return a / b;
}
static size_t yl_bounds(int64_t index, size_t length) {
    if (index < 0 || (uint64_t)index >= (uint64_t)length) {
        yl_runtime_error("array index out of bounds");
    }
    return (size_t)index;
}
static double yl_math_clamp(double value, double low, double high) {
    if (low > high) yl_runtime_error("math.clamp lower bound exceeds upper bound");
    return fmin(fmax(value, low), high);
}
static int64_t yl_string_length(const char *value) {
    return (int64_t)strlen(value);
}
static bool yl_string_contains(const char *value, const char *needle) {
    return strstr(value, needle) != NULL;
}
static bool yl_string_starts_with(const char *value, const char *prefix) {
    size_t prefix_length = strlen(prefix);
    return strncmp(value, prefix, prefix_length) == 0;
}
static bool yl_string_ends_with(const char *value, const char *suffix) {
    size_t value_length = strlen(value);
    size_t suffix_length = strlen(suffix);
    return suffix_length <= value_length &&
           memcmp(value + value_length - suffix_length, suffix, suffix_length) == 0;
}
static const char *yl_string_concat(const char *left, const char *right) {
    return yl_format("%s%s", left, right);
}
static const char *yl_string_replace(const char *value, const char *needle,
                                    const char *replacement) {
    if (!*needle) return yl_format("%s", value);

    size_t value_length = strlen(value);
    size_t needle_length = strlen(needle);
    size_t replacement_length = strlen(replacement);
    size_t match_count = 0;
    const char *scan = value;
    while ((scan = strstr(scan, needle)) != NULL) {
        match_count++;
        scan += needle_length;
    }

    size_t result_length;
    if (replacement_length >= needle_length) {
        size_t delta = replacement_length - needle_length;
        if (delta && match_count > (SIZE_MAX - value_length - 1) / delta) {
            yl_runtime_error("string replacement result is too large");
        }
        result_length = value_length + match_count * delta;
    } else {
        result_length = value_length - match_count * (needle_length - replacement_length);
    }
    if (result_length == SIZE_MAX) {
        yl_runtime_error("string replacement result is too large");
    }

    char *result = malloc(result_length + 1);
    if (!result) yl_runtime_error("out of memory");

    const char *src = value;
    char *dst = result;
    const char *match;
    while ((match = strstr(src, needle)) != NULL) {
        size_t span = (size_t)(match - src);
        memcpy(dst, src, span);
        dst += span;
        memcpy(dst, replacement, replacement_length);
        dst += replacement_length;
        src = match + needle_length;
    }
    strcpy(dst, src);
    return (const char *)yl_track(result);
}
static const char *yl_string_case(const char *value, bool upper) {
    size_t length = strlen(value);
    char *result = malloc(length + 1);
    if (!result) yl_runtime_error("out of memory");

    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        result[i] = (char)(upper ? toupper(ch) : tolower(ch));
    }
    result[length] = '\0';
    return (const char *)yl_track(result);
}
static const char *yl_string_upper(const char *value) { return yl_string_case(value, true); }
static const char *yl_string_lower(const char *value) { return yl_string_case(value, false); }
static const char *yl_read_line(void) {
    size_t capacity = 128U;
    size_t length = 0U;
    char *line = malloc(capacity);
    if (!line) yl_runtime_error("out of memory");
    int ch = 0;
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (length + 1U >= capacity) {
            if (capacity > SIZE_MAX / 2U) {
                free(line);
                yl_runtime_error("input line is too large");
            }
            size_t next = capacity * 2U;
            char *grown = realloc(line, next);
            if (!grown) {
                free(line);
                yl_runtime_error("out of memory");
            }
            line = grown;
            capacity = next;
        }
        line[length++] = (char)ch;
    }
    if (ch == EOF && ferror(stdin)) {
        free(line);
        yl_runtime_error("failed to read standard input");
    }
    while (length > 0U && line[length - 1U] == '\r') length--;
    line[length] = '\0';
    return (const char *)yl_track(line);
}
static const char *yl_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) yl_runtime_error("cannot open file for reading");
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        yl_runtime_error("cannot seek input file");
    }

    long end = ftell(file);
    if (end < 0 || (uintmax_t)end >= (uintmax_t)SIZE_MAX) {
        fclose(file);
        yl_runtime_error("input file is too large");
    }
    rewind(file);

    size_t size = (size_t)end;
    char *data = malloc(size + 1);
    if (!data) {
        fclose(file);
        yl_runtime_error("out of memory");
    }

    size_t got = fread(data, 1, size, file);
    bool failed = ferror(file) != 0 || got != size;
    fclose(file);
    if (failed) {
        free(data);
        yl_runtime_error("failed to read input file");
    }
    if (memchr(data, '\0', size)) {
        free(data);
        yl_runtime_error("binary files are not supported by string I/O");
    }

    data[size] = '\0';
    return (const char *)yl_track(data);
}
static bool yl_write_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    if (!file) return false;

    size_t size = strlen(content);
    bool ok = fwrite(content, 1, size, file) == size;
    if (fclose(file) != 0) ok = false;
    return ok;
}
static bool yl_path_exists(const char *path) {
    struct stat info;
    return path && stat(path, &info) == 0;
}
static bool yl_path_separator(char ch) {
    return ch == '/' || ch == '\\';
}
static const char *yl_path_basename(const char *path) {
    if (!path) return "";
    size_t end = strlen(path);
    while (end > 1U && yl_path_separator(path[end - 1U])) end--;
    size_t start = end;
    while (start > 0U && !yl_path_separator(path[start - 1U])) start--;
    size_t length = end - start;
    bool root = length == 0U && end == 1U && yl_path_separator(path[0]);
    if (root) length = 1U;
    char *result = malloc(length + 1U);
    if (!result) yl_runtime_error("out of memory");
    memcpy(result, path + (root ? 0U : start), length);
    result[length] = '\0';
    return (const char *)yl_track(result);
}
static const char *yl_path_extension(const char *path) {
    const char *base = yl_path_basename(path);
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return "";
    return dot + 1;
}
#ifdef _WIN32
static bool yl_image_open(const char *path) {
    if (!path || !*path || !yl_path_exists(path)) return false;
    HINSTANCE result = ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)result > 32;
}
#else
static bool yl_image_open(const char *path) {
    if (!path || !*path || !yl_path_exists(path)) return false;

    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
#if defined(__APPLE__)
        execlp("open", "open", path, (char *)NULL);
#else
        execlp("xdg-open", "xdg-open", path, (char *)NULL);
#endif
        _exit(127);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;

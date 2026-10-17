#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif
#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
/* Windows SDK has an enum member named TokenType; avoid colliding with
 * YLang's TokenType typedef from lexer.h, which is included above. */
#define TokenType WindowsSdkTokenType
#include <windows.h>
#undef TokenType
#include <direct.h>
#include <io.h>
#include <process.h>
#define YLANG_PATH_SEPARATOR "\\"
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define YLANG_PATH_SEPARATOR "/"
#endif

static int ylang_make_directory(const char *path)
{
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0755);
#endif
}

static int ylang_remove_file(const char *path)
{
#ifdef _WIN32
    return _unlink(path);
#else
    return unlink(path);
#endif
}

static int ylang_remove_directory(const char *path)
{
#ifdef _WIN32
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

static char *ylang_copy_string(const char *value)
{
    size_t length = strlen(value) + 1;
    char *copy = malloc(length);
    if (copy) memcpy(copy, value, length);
    return copy;
}

static char *ylang_create_build_directory(void)
{
#ifdef _WIN32
    char temporary_path[MAX_PATH + 1];
    DWORD length = GetTempPathA((DWORD)sizeof(temporary_path), temporary_path);
    if (length == 0 || length >= sizeof(temporary_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    char reserved_file[MAX_PATH + 1];
    if (GetTempFileNameA(temporary_path, "ylg", 0, reserved_file) == 0) {
        errno = EIO;
        return NULL;
    }

    char directory[MAX_PATH + 8];
    int written = snprintf(directory, sizeof(directory), "%s.dir", reserved_file);
    if (written < 0 || (size_t)written >= sizeof(directory)) {
        (void)DeleteFileA(reserved_file);
        errno = ENAMETOOLONG;
        return NULL;
    }
    if (!CreateDirectoryA(directory, NULL)) {
        (void)DeleteFileA(reserved_file);
        errno = EIO;
        return NULL;
    }
    if (!DeleteFileA(reserved_file)) {
        (void)RemoveDirectoryA(directory);
        errno = EIO;
        return NULL;
    }

    char *result = ylang_copy_string(directory);
    if (!result) {
        (void)RemoveDirectoryA(directory);
        errno = ENOMEM;
    }
    return result;
#else
    const char *pattern = "/tmp/ylang-build-XXXXXX";
    char *directory = ylang_copy_string(pattern);
    if (!directory) {
        errno = ENOMEM;
        return NULL;
    }
    if (!mkdtemp(directory)) {
        int saved_errno = errno;
        free(directory);
        errno = saved_errno;
        return NULL;
    }
    return directory;
#endif
}

/* ------------------------ safe source fixes --------------------------- */
typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

static bool token_spells(const Token *token, const char *word)
{
    size_t length = strlen(word);
    return token->length == length &&
           memcmp(token->start, word, length) == 0;
}

static bool token_list_push(TokenList *list, Token token)
{
    if (list->count == list->capacity) {
        size_t next = list->capacity ? list->capacity * 2 : 64;
        if (next < list->capacity || next > SIZE_MAX / sizeof(*list->items)) {
            return false;
        }
        Token *grown = realloc(list->items, next * sizeof(*grown));
        if (!grown) return false;
        list->items = grown;
        list->capacity = next;
    }
    list->items[list->count++] = token;
    return true;
}

/*
 * YLang 1.0.0 intentionally implements only one high-confidence automatic fix:
 * the common `pritn(...)` typo becomes the built-in `print(...)` statement.
 * Requiring an explicit output path keeps the original file unchanged by
 * default. Strings and comments are ignored because the lexer skips comments
 * and reports string literals as single tokens.
 */
static int run_safe_fixes(const char *source, const char *input_path,
                          const char *output_path)
{
    Lexer lexer = lexer_init(source);
    TokenList tokens = {0};
    bool ok = true;
    for (;;) {
        Token token = lexer_next(&lexer);
        if (!token_list_push(&tokens, token)) {
            fputs("ylang: out of memory while preparing safe fixes\n", stderr);
            free(tokens.items);
            return 70;
        }
        if (token.type == TOKEN_EOF) break;
    }

    bool declares_pritn = false;
    for (size_t i = 0; i + 1 < tokens.count; i++) {
        if (tokens.items[i].type == TOKEN_FUNCTION &&
            tokens.items[i + 1].type == TOKEN_IDENTIFIER &&
            token_spells(&tokens.items[i + 1], "pritn")) {
            declares_pritn = true;
            break;
        }
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "ylang: cannot write '%s': %s\n",
                output_path, strerror(errno));
        free(tokens.items);
        return 73;
    }

    size_t copied = 0;
    unsigned fixes = 0;
    for (size_t i = 0; i + 1 < tokens.count; i++) {
        Token *token = &tokens.items[i];
        if (declares_pritn || token->type != TOKEN_IDENTIFIER ||
            !token_spells(token, "pritn") ||
            tokens.items[i + 1].type != TOKEN_LEFT_PAREN) {
            continue;
        }

        ptrdiff_t difference = token->start - source;
        if (difference < 0) continue;
        size_t offset = (size_t)difference;
        if (offset < copied || offset > strlen(source) ||
            token->length > strlen(source) - offset) {
            continue;
        }
        if (fwrite(source + copied, 1, offset - copied, out) != offset - copied ||
            fwrite("print", 1, 5, out) != 5) {
            ok = false;
            break;
        }
        copied = offset + token->length;
        fixes++;
        fprintf(stderr,
                "ylang fix: %s:%zu:%zu: replaced 'pritn' with 'print'\n",
                input_path, token->line, token->column);
    }

    size_t source_length = strlen(source);
    if (ok && fwrite(source + copied, 1, source_length - copied, out) != source_length - copied) {
        ok = false;
    }
    if (fclose(out) != 0) ok = false;
    free(tokens.items);

    if (!ok) {
        fprintf(stderr, "ylang: failed while writing '%s'\n", output_path);
        return 74;
    }
    if (fixes == 0) {
        printf("YLang fix: no safe automatic fixes found; source copied unchanged to %s.\n",
               output_path);
    } else {
        printf("YLang fix: applied %u safe fix(es); wrote %s.\n", fixes, output_path);
        printf("Run 'ylang check %s' to verify remaining issues.\n", output_path);
    }
    return 0;
}

/* -------------------------------- driver ------------------------------- */
static char *read_source_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    char *data = malloc((size_t)size + 1);
    if (!data) { fclose(file); return NULL; }
    size_t got = fread(data, 1, (size_t)size, file);
    if (got != (size_t)size || ferror(file)) {
        free(data); fclose(file); return NULL;
    }
    data[got] = '\0';
    fclose(file);
    *length = got;
    return data;
}


enum {
    YLANG_MAX_MODULES = 128,
    YLANG_MAX_IMPORT_DEPTH = 32,
    YLANG_MAX_PROJECT_SOURCE = 16 * 1024 * 1024
};

typedef struct {
    char *path;
    char *display_path;
    char *source;
    size_t length;
    char *processed;
    bool visiting;
    bool done;
} ImportModule;

typedef struct {
    ImportModule modules[YLANG_MAX_MODULES];
    size_t module_count;
    size_t order[YLANG_MAX_MODULES];
    size_t order_count;
    size_t total_source_bytes;
} ImportContext;

typedef struct {
    char *module_path;
    char **symbols;
    size_t symbol_count;
} ImportDirective;

typedef struct {
    char *source;
    size_t length;
    SourceUnit *units;
    size_t unit_count;
} SourceBundle;

static char *copy_source_bytes(const char *source, size_t length)
{
    if (length == SIZE_MAX) return NULL;
    char *copy = malloc(length + 1U);
    if (!copy) return NULL;
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

static void import_directive_destroy(ImportDirective *directive)
{
    if (!directive) return;
    free(directive->module_path);
    for (size_t i = 0; i < directive->symbol_count; i++)
        free(directive->symbols[i]);
    free(directive->symbols);
    memset(directive, 0, sizeof(*directive));
}

static void source_bundle_destroy(SourceBundle *bundle)
{
    if (!bundle) return;
    free(bundle->source);
    for (size_t i = 0; i < bundle->unit_count; i++)
        free((char *)bundle->units[i].filename);
    free(bundle->units);
    memset(bundle, 0, sizeof(*bundle));
}

static void import_context_destroy(ImportContext *context)
{
    for (size_t i = 0; i < context->module_count; i++) {
        free(context->modules[i].path);
        free(context->modules[i].display_path);
        free(context->modules[i].source);
        free(context->modules[i].processed);
    }
    memset(context, 0, sizeof(*context));
}

static bool path_is_absolute(const char *path)
{
#ifdef _WIN32
    return path[0] == '\\' || path[0] == '/' ||
           (isalpha((unsigned char)path[0]) && path[1] == ':');
#else
    return path[0] == '/';
#endif
}

static char *canonicalize_module_path(const char *path)
{
    char resolved[4096];
#ifdef _WIN32
    if (!_fullpath(resolved, path, sizeof(resolved))) return NULL;
#else
    if (!realpath(path, resolved)) return NULL;
#endif
    return ylang_copy_string(resolved);
}

static char *resolve_module_path(const char *requested,
                                 const char *importer_path)
{
    char candidate[8192];
    if (path_is_absolute(requested)) {
        int written = snprintf(candidate, sizeof(candidate), "%s", requested);
        if (written < 0 || (size_t)written >= sizeof(candidate)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
    } else {
        const char *last_slash = strrchr(importer_path, '/');
#ifdef _WIN32
        const char *last_backslash = strrchr(importer_path, '\\');
        if (!last_slash || (last_backslash && last_backslash > last_slash))
            last_slash = last_backslash;
#endif
        size_t directory_length = last_slash
            ? (size_t)(last_slash - importer_path + 1) : 0U;
        if (directory_length + strlen(requested) + 1U > sizeof(candidate)) {
            errno = ENAMETOOLONG;
            return NULL;
        }
        if (directory_length) memcpy(candidate, importer_path, directory_length);
        memcpy(candidate + directory_length, requested, strlen(requested) + 1U);
    }
    return canonicalize_module_path(candidate);
}

static bool import_identifier_char(char ch, bool first)
{
    unsigned char value = (unsigned char)ch;
    if ((value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z') || value == '_') return true;
    return !first && value >= '0' && value <= '9';
}

static void skip_import_space(const char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t' ||
           **cursor == '\r' || **cursor == '\f') {
        (*cursor)++;
    }
}

static bool import_add_symbol(ImportDirective *directive,
                              const char *name, size_t length)
{
    if (length == 0 || length > 127U ||
        directive->symbol_count >= 64U ||
        directive->symbol_count >= SIZE_MAX / sizeof(*directive->symbols)) {
        return false;
    }
    char **grown = realloc(directive->symbols,
                           (directive->symbol_count + 1U) * sizeof(*grown));
    if (!grown) return false;
    directive->symbols = grown;
    char *copy = copy_source_bytes(name, length);
    if (!copy) return false;
    directive->symbols[directive->symbol_count++] = copy;
    return true;
}

/* Returns 0 for an ordinary line, 1 for a valid import, and -1 for an invalid import. */
static int parse_import_directive(const char *line, size_t length,
                                  ImportDirective *directive,
                                  char *error, size_t error_capacity)
{
    memset(directive, 0, sizeof(*directive));
    const char *cursor = line;
    const char *end = line + length;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) cursor++;

    if ((size_t)(end - cursor) < 4U || memcmp(cursor, "from", 4U) != 0 ||
        (cursor + 4U < end && cursor[4] != ' ' && cursor[4] != '\t')) {
        return 0;
    }
    cursor += 4U;
    skip_import_space(&cursor);
    if (cursor >= end) goto malformed;

    const char *path_start;
    size_t path_length;
    if (*cursor == '"' || *cursor == '\'') {
        char quote = *cursor++;
        path_start = cursor;
        while (cursor < end && *cursor != quote) cursor++;
        if (cursor >= end) goto malformed;
        path_length = (size_t)(cursor - path_start);
        cursor++;
    } else {
        path_start = cursor;
        while (cursor < end && *cursor != ' ' && *cursor != '\t' &&
               *cursor != '\r' && *cursor != ';') cursor++;
        path_length = (size_t)(cursor - path_start);
    }
    if (path_length == 0 || path_length > 4095U) goto malformed;

    if (cursor >= end || (*cursor != ' ' && *cursor != '\t')) goto malformed;
    skip_import_space(&cursor);
    if ((size_t)(end - cursor) < 6U || memcmp(cursor, "import", 6U) != 0 ||
        (cursor + 6U < end && cursor[6] != ' ' && cursor[6] != '\t')) {
        goto malformed;
    }
    cursor += 6U;
    skip_import_space(&cursor);

    directive->module_path = copy_source_bytes(path_start, path_length);
    if (!directive->module_path) goto out_of_memory;

    for (;;) {
        skip_import_space(&cursor);
        if (cursor >= end || *cursor == '\r' ||
            (cursor + 1 < end && cursor[0] == '/' && cursor[1] == '/')) {
            if (directive->symbol_count == 0) goto malformed;
            return 1;
        }
        if (*cursor == ';') {
            cursor++;
            skip_import_space(&cursor);
            if (cursor < end && !(cursor + 1 < end &&
                                  cursor[0] == '/' && cursor[1] == '/')) {
                goto malformed;
            }
            if (directive->symbol_count == 0) goto malformed;
            return 1;
        }

        const char *name_start = cursor;
        if (!import_identifier_char(*cursor, true)) goto malformed;
        cursor++;
        while (cursor < end && import_identifier_char(*cursor, false)) cursor++;
        size_t name_length = (size_t)(cursor - name_start);
        for (size_t i = 0; i < directive->symbol_count; i++) {
            size_t existing_length = strlen(directive->symbols[i]);
            if (existing_length == name_length &&
                memcmp(directive->symbols[i], name_start, name_length) == 0) {
                (void)snprintf(error, error_capacity,
                               "duplicate imported function '%.*s'",
                               (int)name_length, name_start);
                import_directive_destroy(directive);
                return -1;
            }
        }
        if (!import_add_symbol(directive, name_start, name_length))
            goto out_of_memory;

        skip_import_space(&cursor);
        if (cursor < end && *cursor == '(') {
            cursor++;
            skip_import_space(&cursor);
            if (cursor >= end || *cursor != ')') goto malformed;
            cursor++;
        }
        skip_import_space(&cursor);
        if (cursor >= end ||
            (cursor + 1 < end && cursor[0] == '/' && cursor[1] == '/')) {
            return directive->symbol_count ? 1 : -1;
        }
        if (*cursor == ';') {
            cursor++;
            skip_import_space(&cursor);
            if (cursor < end && !(cursor + 1 < end &&
                                  cursor[0] == '/' && cursor[1] == '/')) {
                goto malformed;
            }
            return 1;
        }
        if (*cursor != ',') goto malformed;
        cursor++;
        skip_import_space(&cursor);
        if (cursor >= end) goto malformed;
    }

malformed:
    (void)snprintf(error, error_capacity,
                   "invalid import declaration; expected 'from module.yl import function_name()'");
    import_directive_destroy(directive);
    return -1;

out_of_memory:
    (void)snprintf(error, error_capacity,
                   "out of memory while parsing an import declaration");
    import_directive_destroy(directive);
    return -1;
}

static bool module_declares_function(const char *source, const char *name)
{
    Lexer lexer = lexer_init(source);
    for (;;) {
        Token token = lexer_next(&lexer);
        if (token.type == TOKEN_EOF) return false;
        if (token.type != TOKEN_FUNCTION) continue;

        Token candidate;
        do {
            candidate = lexer_next(&lexer);
        } while (candidate.type == TOKEN_NEWLINE);
        if (candidate.type == TOKEN_IDENTIFIER &&
            candidate.length == strlen(name) &&
            memcmp(candidate.start, name, candidate.length) == 0) {
            return true;
        }
    }
}

static size_t find_loaded_module(const ImportContext *context,
                                 const char *canonical_path)
{
    for (size_t i = 0; i < context->module_count; i++) {
        if (strcmp(context->modules[i].path, canonical_path) == 0) return i;
    }
    return SIZE_MAX;
}

static size_t load_import_module(ImportContext *context,
                                 const char *requested_path,
                                 const char *importer_path)
{
    char *canonical_path = resolve_module_path(requested_path, importer_path);
    if (!canonical_path) {
        fprintf(stderr,
                "ylang: cannot resolve imported module '%s' from '%s': %s\n",
                requested_path, importer_path, strerror(errno));
        return SIZE_MAX;
    }

    size_t existing = find_loaded_module(context, canonical_path);
    if (existing != SIZE_MAX) {
        free(canonical_path);
        return existing;
    }
    if (context->module_count >= YLANG_MAX_MODULES) {
        fprintf(stderr, "ylang: project exceeds the %d-module import limit\n",
                YLANG_MAX_MODULES);
        free(canonical_path);
        return SIZE_MAX;
    }

    size_t module_length = 0;
    char *module_source = read_source_file(canonical_path, &module_length);
    if (!module_source) {
        fprintf(stderr, "ylang: cannot read imported module '%s': %s\n",
                canonical_path, strerror(errno));
        free(canonical_path);
        return SIZE_MAX;
    }
    if (module_length > (size_t)YLANG_MAX_PROJECT_SOURCE -
                        context->total_source_bytes) {
        fprintf(stderr, "ylang: combined project source exceeds the 16 MiB limit\n");
        free(module_source);
        free(canonical_path);
        return SIZE_MAX;
    }

    size_t index = context->module_count++;
    ImportModule *module = &context->modules[index];
    memset(module, 0, sizeof(*module));
    module->path = canonical_path;
    module->display_path = ylang_copy_string(canonical_path);
    module->source = module_source;
    module->length = module_length;
    context->total_source_bytes += module_length;
    if (!module->display_path) {
        fprintf(stderr, "ylang: out of memory while tracking imported module\n");
        return SIZE_MAX;
    }
    return index;
}

static void update_brace_depth(const char *line, size_t length, int *depth)
{
    size_t i = 0;
    while (i < length) {
        char ch = line[i];
        if (ch == '/' && i + 1U < length && line[i + 1U] == '/') return;
        if (ch == '"' || ch == '\'') {
            char quote = ch;
            i++;
            while (i < length) {
                if (line[i] == '\\') {
                    i += i + 1U < length ? 2U : 1U;
                } else if (line[i++] == quote) {
                    break;
                }
            }
            continue;
        }
        if (ch == '{') (*depth)++;
        else if (ch == '}' && *depth > 0) (*depth)--;
        i++;
    }
}

static bool process_import_module(ImportContext *context, size_t module_index,
                                  unsigned depth)
{
    if (depth > YLANG_MAX_IMPORT_DEPTH) {
        fprintf(stderr, "ylang: module import depth exceeds the %d-level limit\n",
                YLANG_MAX_IMPORT_DEPTH);
        return false;
    }
    ImportModule *module = &context->modules[module_index];
    if (module->done) return true;
    if (module->visiting) {
        fprintf(stderr, "error[E1102]: cyclic import detected for module '%s'\n",
                module->display_path ? module->display_path : module->path);
        return false;
    }

    module->visiting = true;
    module->processed = copy_source_bytes(module->source, module->length);
    if (!module->processed) {
        fprintf(stderr, "ylang: out of memory while preparing module '%s'\n",
                module->display_path);
        return false;
    }

    int brace_depth = 0;
    size_t offset = 0;
    size_t line_number = 1U;
    while (offset < module->length) {
        size_t line_end = offset;
        while (line_end < module->length && module->source[line_end] != '\n')
            line_end++;
        size_t content_end = line_end;
        ImportDirective directive = {0};
        char parse_error[256] = "";
        int import_line = brace_depth == 0
            ? parse_import_directive(module->source + offset,
                                     content_end - offset, &directive,
                                     parse_error, sizeof(parse_error))
            : 0;

        if (import_line < 0) {
            fprintf(stderr, "error[E1100]: %s\n  --> %s:%zu:1\n\n",
                    parse_error, module->display_path, line_number);
            return false;
        }
        if (import_line == 1) {
            size_t imported_index = load_import_module(
                context, directive.module_path, module->path);
            if (imported_index == SIZE_MAX) {
                import_directive_destroy(&directive);
                return false;
            }
            if (!process_import_module(context, imported_index, depth + 1U)) {
                import_directive_destroy(&directive);
                return false;
            }
            ImportModule *imported = &context->modules[imported_index];
            for (size_t i = 0; i < directive.symbol_count; i++) {
                if (!module_declares_function(imported->source,
                                              directive.symbols[i])) {
                    fprintf(stderr,
                            "error[E1101]: module '%s' does not define function '%s'\n"
                            "  --> %s:%zu:1\n\n",
                            imported->display_path, directive.symbols[i],
                            module->display_path, line_number);
                    import_directive_destroy(&directive);
                    return false;
                }
            }
            memset(module->processed + offset, ' ', content_end - offset);
            import_directive_destroy(&directive);
        } else {
            update_brace_depth(module->source + offset, content_end - offset,
                               &brace_depth);
        }

        offset = line_end < module->length ? line_end + 1U : line_end;
        line_number++;
    }

    module->visiting = false;
    module->done = true;
    if (context->order_count >= YLANG_MAX_MODULES) {
        fprintf(stderr, "ylang: project has too many imported modules\n");
        return false;
    }
    context->order[context->order_count++] = module_index;
    return true;
}

static bool build_source_bundle(const char *input_path,
                                const char *source, size_t source_length,
                                SourceBundle *bundle)
{
    memset(bundle, 0, sizeof(*bundle));
    ImportContext *context = calloc(1U, sizeof(*context));
    if (!context) {
        fputs("ylang: out of memory while preparing source modules\n", stderr);
        return false;
    }

    char *canonical_root = canonicalize_module_path(input_path);
    if (!canonical_root) canonical_root = ylang_copy_string(input_path);
    char *root_source = copy_source_bytes(source, source_length);
    char *root_display = ylang_copy_string(input_path);
    if (!canonical_root || !root_source || !root_display ||
        source_length > (size_t)YLANG_MAX_PROJECT_SOURCE) {
        fputs("ylang: cannot prepare project source or source exceeds the 16 MiB limit\n",
              stderr);
        free(canonical_root);
        free(root_source);
        free(root_display);
        free(context);
        return false;
    }

    ImportModule *root = &context->modules[0];
    root->path = canonical_root;
    root->display_path = root_display;
    root->source = root_source;
    root->length = source_length;
    context->module_count = 1U;
    context->total_source_bytes = source_length;

    bool okay = process_import_module(context, 0U, 0U);
    size_t combined_length = 0;
    if (okay) {
        for (size_t i = 0; i < context->order_count; i++) {
            ImportModule *module = &context->modules[context->order[i]];
            if (module->length > (size_t)YLANG_MAX_PROJECT_SOURCE -
                                 combined_length) {
                okay = false;
                fprintf(stderr, "ylang: combined project source exceeds the 16 MiB limit\n");
                break;
            }
            combined_length += module->length;
            if (i + 1U < context->order_count) {
                if (combined_length >= (size_t)YLANG_MAX_PROJECT_SOURCE) {
                    okay = false;
                    fprintf(stderr, "ylang: combined project source exceeds the 16 MiB limit\n");
                    break;
                }
                combined_length++;
            }
        }
    }

    if (okay) {
        bundle->source = malloc(combined_length + 1U);
        bundle->units = calloc(context->order_count, sizeof(*bundle->units));
        if (!bundle->source || !bundle->units) {
            okay = false;
            fputs("ylang: out of memory while combining source modules\n", stderr);
        }
    }
    if (okay) {
        size_t combined_offset = 0U;
        for (size_t i = 0; i < context->order_count; i++) {
            ImportModule *module = &context->modules[context->order[i]];
            bundle->units[i].filename = ylang_copy_string(module->display_path);
            bundle->units[i].offset = combined_offset;
            bundle->units[i].length = module->length;
            if (!bundle->units[i].filename) {
                okay = false;
                break;
            }
            bundle->unit_count = i + 1U;
            memcpy(bundle->source + combined_offset, module->processed, module->length);
            combined_offset += module->length;
            if (i + 1U < context->order_count)
                bundle->source[combined_offset++] = '\n';
        }
        if (okay) {
            bundle->source[combined_offset] = '\0';
            bundle->length = combined_offset;
            bundle->unit_count = context->order_count;
        }
    }

    import_context_destroy(context);
    free(context);
    if (!okay) source_bundle_destroy(bundle);
    return okay;
}


static int run_native_compiler(const char *cc, const char *c_path,
                               const char *output_path, bool uses_http)
{
#ifdef _WIN32
    const char *arguments_plain[] = {
        cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-function",
        "-O2", "-g", "-fstack-protector-strong",
        c_path, "-lm", "-o", output_path, NULL
    };
    const char *arguments_http[] = {
        cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-function",
        "-O2", "-g", "-fstack-protector-strong",
        c_path, "-lcurl", "-lm", "-o", output_path, NULL
    };
    intptr_t status = _spawnvp(_P_WAIT, cc,
                               uses_http ? arguments_http : arguments_plain);
    if (status == -1) {
        fprintf(stderr, "ylang: cannot execute native compiler '%s': %s\n",
                cc, strerror(errno));
        return errno == ENOENT ? 127 : 1;
    }
    return (int)status;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "ylang: could not start native compiler: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        const char *sanitize_native = getenv("YLANG_SANITIZE_NATIVE");
        if (sanitize_native && strcmp(sanitize_native, "1") == 0) {
            if (uses_http) {
                execlp(cc, cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic",
                       "-Wno-unused-function", "-O2", "-g", "-D_FORTIFY_SOURCE=3",
                       "-fstack-protector-strong", "-fPIE",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                       c_path, "-lcurl", "-lm", "-pie", "-Wl,-z,relro,-z,now",
                       "-o", output_path, (char *)NULL);
            } else {
                execlp(cc, cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic",
                       "-Wno-unused-function", "-O2", "-g", "-D_FORTIFY_SOURCE=3",
                       "-fstack-protector-strong", "-fPIE",
                       "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                       c_path, "-lm", "-pie", "-Wl,-z,relro,-z,now",
                       "-o", output_path, (char *)NULL);
            }
            fprintf(stderr, "ylang: cannot execute '%s': %s\n", cc, strerror(errno));
            _exit(127);
        }

        if (uses_http) {
            execlp(cc, cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic",
                   "-Wno-unused-function", "-O2", "-g", "-D_FORTIFY_SOURCE=3",
                   "-fstack-protector-strong", "-fPIE", c_path, "-lcurl", "-lm",
                   "-pie", "-Wl,-z,relro,-z,now", "-o", output_path, (char *)NULL);
        } else {
            execlp(cc, cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic",
                   "-Wno-unused-function", "-O2", "-g", "-D_FORTIFY_SOURCE=3",
                   "-fstack-protector-strong", "-fPIE", c_path, "-lm",
                   "-pie", "-Wl,-z,relro,-z,now", "-o", output_path, (char *)NULL);
        }
        fprintf(stderr, "ylang: cannot execute '%s': %s\n", cc, strerror(errno));
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fprintf(stderr, "ylang: waitpid failed: %s\n", strerror(errno));
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
#endif
}


static int run_native_program(const char *executable)
{
#ifdef _WIN32
    const char *arguments[] = { executable, NULL };
    intptr_t status = _spawnv(_P_WAIT, executable, arguments);
    if (status == -1) {
        fprintf(stderr, "ylang: cannot run generated program '%s': %s\n",
                executable, strerror(errno));
        return errno == ENOENT ? 127 : 1;
    }
    return (int)status;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "ylang: could not start generated program: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        char *const arguments[] = { (char *)executable, NULL };
        execv(executable, arguments);
        fprintf(stderr, "ylang: cannot execute generated program '%s': %s\n",
                executable, strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        fprintf(stderr, "ylang: waitpid failed while running program: %s\n",
                strerror(errno));
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}

int ylang_run(const char *command, const char *input_path,
              const char *output_path, const char *cc)
{
    size_t source_length = 0;
    char *source = read_source_file(input_path, &source_length);
    if (!source) {
        fprintf(stderr, "ylang: cannot read '%s': %s\n", input_path, strerror(errno));
        return 66;
    }

    Compiler compiler;
    memset(&compiler, 0, sizeof(compiler));
    compiler.filename = (char *)input_path;
    compiler.source = source;
    compiler.source_length = source_length;
    if (strcmp(command, "fix") == 0) {
        if (!output_path) {
            fputs("ylang: fix requires an explicit output path with -o\n", stderr);
            free(source);
            return 64;
        }
        int fix_result = run_safe_fixes(source, input_path, output_path);
        free(source);
        return fix_result;
    }

    SourceBundle source_bundle = {0};
    if (!build_source_bundle(input_path, source, source_length, &source_bundle)) {
        free(source);
        return 66;
    }
    free(source);
    source = source_bundle.source;
    source_length = source_bundle.length;
    compiler.source = source;
    compiler.source_length = source_length;
    compiler.source_units = source_bundle.units;
    compiler.source_unit_count = source_bundle.unit_count;

    (void)parse_program(&compiler);
    if (!compiler.had_error) (void)check_program(&compiler);

    if (compiler.had_error) {
        if (strcmp(command, "check") == 0) {
            fprintf(stderr, "ylang: check failed with %u error(s).\n",
                    compiler.error_count);
        } else if (strcmp(command, "emit-c") == 0) {
            fprintf(stderr, "ylang: emit-c failed with %u error(s); no C source was generated.\n",
                    compiler.error_count);
        } else {
            fprintf(stderr, "ylang: build failed with %u error(s); no executable was produced.\n",
                    compiler.error_count);
        }
        arena_destroy(&compiler.arena);
        source_bundle_destroy(&source_bundle);
        return 1;
    }

    if (strcmp(command, "check") == 0) {
        printf("YLang check: no errors found (%s).\n", input_path);
        arena_destroy(&compiler.arena);
        source_bundle_destroy(&source_bundle);
        return 0;
    }

    const bool emit_c_command = strcmp(command, "emit-c") == 0;
    const bool run_command = strcmp(command, "run") == 0;
    const char *c_path = output_path;
    char *generated_path = NULL;
    char *temporary_directory = NULL;
    char *run_target = NULL;

    if (emit_c_command) {
        if (!c_path) {
            if (ylang_make_directory("build") != 0 && errno != EEXIST) {
                fprintf(stderr, "ylang: cannot create build directory: %s\n", strerror(errno));
                arena_destroy(&compiler.arena);
                source_bundle_destroy(&source_bundle);
                return 73;
            }
            c_path = "build/ylang-generated.c";
        }
    } else {
        temporary_directory = ylang_create_build_directory();
        if (!temporary_directory) {
            fprintf(stderr, "ylang: cannot create a temporary build directory: %s\n", strerror(errno));
            arena_destroy(&compiler.arena);
            source_bundle_destroy(&source_bundle);
            return 73;
        }
        size_t path_size = strlen(temporary_directory) + sizeof(YLANG_PATH_SEPARATOR "generated.c");
        generated_path = malloc(path_size);
        if (!generated_path) {
            fputs("ylang: out of memory while preparing native build\n", stderr);
            (void)ylang_remove_directory(temporary_directory);
            free(temporary_directory);
            arena_destroy(&compiler.arena);
            source_bundle_destroy(&source_bundle);
            return 70;
        }
        (void)snprintf(generated_path, path_size, "%s%s%s",
                       temporary_directory, YLANG_PATH_SEPARATOR, "generated.c");
        c_path = generated_path;
        if (run_command) {
#ifdef _WIN32
            const char *run_name = "ylang-run.exe";
#else
            const char *run_name = "ylang-run";
#endif
            size_t target_path_size = strlen(temporary_directory) +
                strlen(YLANG_PATH_SEPARATOR) + strlen(run_name) + 1;
            run_target = malloc(target_path_size);
            if (!run_target) {
                fputs("ylang: out of memory while preparing run target\\n", stderr);
                (void)ylang_remove_directory(temporary_directory);
                free(generated_path);
                free(temporary_directory);
                arena_destroy(&compiler.arena);
                source_bundle_destroy(&source_bundle);
                return 70;
            }
            (void)snprintf(run_target, target_path_size, "%s%s%s",
                           temporary_directory, YLANG_PATH_SEPARATOR, run_name);
        }
    }

    if (!generate_c(&compiler, c_path)) {
        if (temporary_directory) {
            (void)ylang_remove_file(c_path);
            if (run_target) (void)ylang_remove_file(run_target);
            (void)ylang_remove_directory(temporary_directory);
        }
        free(run_target);
        free(generated_path);
        free(temporary_directory);
        arena_destroy(&compiler.arena);
        source_bundle_destroy(&source_bundle);
        return 1;
    }

    int result = 0;
    if (emit_c_command) {
        printf("Generated C source: %s\n", c_path);
    } else {
        const char *target;
        if (run_command) {
            target = run_target;
        } else if (output_path) {
            target = output_path;
        } else {
#ifdef _WIN32
            target = "a.exe";
#else
            target = "a.out";
#endif
        }
        result = run_native_compiler(cc ? cc : "gcc", c_path, target, compiler.uses_http);
        if (result == 0 && run_command) {
            result = run_native_program(target);
        } else if (result == 0) {
            printf("Build succeeded: %s\n", target);
        } else {
            fprintf(stderr, "ylang: native compilation failed (exit %d).\n", result);
        }
        (void)ylang_remove_file(c_path);
        if (run_target) (void)ylang_remove_file(run_target);
        (void)ylang_remove_directory(temporary_directory);
    }

    free(run_target);
    free(generated_path);
    free(temporary_directory);
    arena_destroy(&compiler.arena);
    source_bundle_destroy(&source_bundle);
    return result;
}

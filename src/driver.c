#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

static int run_native_compiler(const char *cc, const char *c_path,
                               const char *output_path)
{
#ifdef _WIN32
    const char *arguments[] = {
        cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-function",
        "-O2", "-g", "-fstack-protector-strong",
        c_path, "-lm", "-o", output_path, NULL
    };
    intptr_t status = _spawnvp(_P_WAIT, cc, arguments);
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
        execlp(cc, cc, "-std=c17", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-function",
               "-O2", "-g", "-D_FORTIFY_SOURCE=3", "-fstack-protector-strong", "-fPIE",
               c_path, "-lm", "-pie", "-Wl,-z,relro,-z,now", "-o", output_path, (char *)NULL);
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
        free(source);
        return 1;
    }

    if (strcmp(command, "check") == 0) {
        printf("YLang check: no errors found (%s).\n", input_path);
        arena_destroy(&compiler.arena);
        free(source);
        return 0;
    }

    const bool emit_c_command = strcmp(command, "emit-c") == 0;
    const char *c_path = output_path;
    char *generated_path = NULL;
    char *temporary_directory = NULL;

    if (emit_c_command) {
        if (!c_path) {
            if (ylang_make_directory("build") != 0 && errno != EEXIST) {
                fprintf(stderr, "ylang: cannot create build directory: %s\n", strerror(errno));
                arena_destroy(&compiler.arena);
                free(source);
                return 73;
            }
            c_path = "build/ylang-generated.c";
        }
    } else {
        temporary_directory = ylang_create_build_directory();
        if (!temporary_directory) {
            fprintf(stderr, "ylang: cannot create a temporary build directory: %s\n", strerror(errno));
            arena_destroy(&compiler.arena);
            free(source);
            return 73;
        }
        size_t path_size = strlen(temporary_directory) + sizeof(YLANG_PATH_SEPARATOR "generated.c");
        generated_path = malloc(path_size);
        if (!generated_path) {
            fputs("ylang: out of memory while preparing native build\n", stderr);
            (void)ylang_remove_directory(temporary_directory);
            free(temporary_directory);
            arena_destroy(&compiler.arena);
            free(source);
            return 70;
        }
        (void)snprintf(generated_path, path_size, "%s%s%s",
                       temporary_directory, YLANG_PATH_SEPARATOR, "generated.c");
        c_path = generated_path;
    }

    if (!generate_c(&compiler, c_path)) {
        if (temporary_directory) {
            (void)ylang_remove_file(c_path);
            (void)ylang_remove_directory(temporary_directory);
        }
        free(generated_path);
        free(temporary_directory);
        arena_destroy(&compiler.arena);
        free(source);
        return 1;
    }

    int result = 0;
    if (emit_c_command) {
        printf("Generated C source: %s\n", c_path);
    } else {
        const char *target = output_path ? output_path :
#ifdef _WIN32
            "a.exe";
#else
            "a.out";
#endif
        result = run_native_compiler(cc ? cc : "gcc", c_path, target);
        if (result == 0) printf("Build succeeded: %s\n", target);
        else fprintf(stderr, "ylang: native compilation failed (exit %d).\n", result);
        (void)ylang_remove_file(c_path);
        (void)ylang_remove_directory(temporary_directory);
    }

    free(generated_path);
    free(temporary_directory);
    arena_destroy(&compiler.arena);
    free(source);
    return result;
}

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "ylang/ir.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#ifndef R_OK
#define R_OK 4
#endif
#define access _access
#define popen _popen
#define pclose _pclose
#define unlink _unlink
#define rmdir _rmdir
#define ylang_mkdir(path, mode) _mkdir(path)
#else
#include <sys/wait.h>
#include <unistd.h>
#define ylang_mkdir(path, mode) mkdir(path, mode)
#endif


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

static bool paths_refer_same_file(const char *input_path, const char *output_path)
{
    if (!input_path || !output_path) return false;
    if (strcmp(input_path, output_path) == 0) return true;

    struct stat input_stat;
    struct stat output_stat;
    return stat(input_path, &input_stat) == 0 &&
           stat(output_path, &output_stat) == 0 &&
           input_stat.st_dev == output_stat.st_dev &&
           input_stat.st_ino == output_stat.st_ino;
}

#ifndef YLANG_SHARE_DIR
#define YLANG_SHARE_DIR "/usr/local/share/ylang"
#endif
#ifndef YLANG_INCLUDE_DIR
#define YLANG_INCLUDE_DIR "/usr/local/include"
#endif

static bool resolve_gui_paths(char *runtime_path, size_t runtime_capacity,
                              char *include_path, size_t include_capacity)
{
    if (access("stdlib/gui/gui.c", R_OK) == 0 &&
        access("include/ylang/gui.h", R_OK) == 0) {
        if (snprintf(runtime_path, runtime_capacity, "%s", "stdlib/gui/gui.c") >=
                (int)runtime_capacity ||
            snprintf(include_path, include_capacity, "%s", "include") >=
                (int)include_capacity) return false;
        return true;
    }
    if (snprintf(runtime_path, runtime_capacity, "%s/stdlib/gui/gui.c",
                 YLANG_SHARE_DIR) >= (int)runtime_capacity ||
        snprintf(include_path, include_capacity, "%s", YLANG_INCLUDE_DIR) >=
                 (int)include_capacity) return false;
    return access(runtime_path, R_OK) == 0 && access(include_path, R_OK) == 0;
}


static char *create_temporary_directory(const char *prefix)
{
#ifdef _WIN32
    const char *base = getenv("TEMP");
    if (!base || !*base) base = getenv("TMP");
    if (!base || !*base) base = ".";
    size_t base_length = strlen(base);
    bool has_separator = base_length > 0U &&
        (base[base_length - 1U] == '/' || base[base_length - 1U] == '\\');
    size_t capacity = base_length + strlen(prefix) + 80U;
    char *path = malloc(capacity);
    if (!path) { errno = ENOMEM; return NULL; }
    for (unsigned int attempt = 0; attempt < 1000U; attempt++) {
        int written = snprintf(path, capacity, "%s%s%s-%lu-%u",
            base, has_separator ? "" : "\\", prefix,
            (unsigned long)_getpid(), attempt);
        if (written < 0 || (size_t)written >= capacity) {
            free(path);
            errno = ENAMETOOLONG;
            return NULL;
        }
        if (_mkdir(path) == 0) return path;
        if (errno != EEXIST) {
            int saved = errno;
            free(path);
            errno = saved;
            return NULL;
        }
    }
    free(path);
    errno = EEXIST;
    return NULL;
#else
    const char *base = getenv("TMPDIR");
    if (!base || !*base) base = "/tmp";
    size_t capacity = strlen(base) + strlen(prefix) + 10U;
    char *path = malloc(capacity);
    if (!path) { errno = ENOMEM; return NULL; }
    int written = snprintf(path, capacity, "%s/%s-XXXXXX", base, prefix);
    if (written < 0 || (size_t)written >= capacity || !mkdtemp(path)) {
        int saved = errno ? errno : ENAMETOOLONG;
        free(path);
        errno = saved;
        return NULL;
    }
    return path;
#endif
}

static bool read_sdl2_flags(char *storage, size_t capacity,
                            char **flags, size_t flag_capacity,
                            size_t *flag_count)
{
    FILE *pipe = popen("pkg-config --cflags --libs sdl2", "r");
    if (!pipe) {
        fputs("ylang: could not start pkg-config while preparing the GUI build.\n", stderr);
        return false;
    }
    size_t used = 0;
    while (used + 1 < capacity) {
        size_t got = fread(storage + used, 1, capacity - used - 1, pipe);
        used += got;
        if (got == 0) break;
    }
    storage[used] = '\0';
    int status = pclose(pipe);
#ifdef _WIN32
    if (status == -1 || status != 0 || used == 0 || used + 1 >= capacity) {
#else
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        used == 0 || used + 1 >= capacity) {
#endif
        fputs("ylang: GUI build requires SDL2 development files and pkg-config (Fedora: SDL2-devel pkgconf-pkg-config; Debian/Ubuntu: libsdl2-dev pkg-config; macOS: brew install sdl2 pkg-config).\n", stderr);
        return false;
    }
    *flag_count = 0;
    for (char *part = strtok(storage, " \t\r\n");
         part != NULL; part = strtok(NULL, " \t\r\n")) {
        if (*flag_count >= flag_capacity) {
            fputs("ylang: too many SDL2 compiler flags returned by pkg-config.\n", stderr);
            return false;
        }
        flags[(*flag_count)++] = part;
    }
    return *flag_count > 0;
}

static bool read_http_flags(char *storage, size_t capacity,
                            char **flags, size_t flag_capacity,
                            size_t *flag_count)
{
    FILE *pipe = popen("pkg-config --cflags --libs libcurl", "r");
    if (!pipe) {
        fputs("ylang: could not start pkg-config while preparing the HTTP build.\n", stderr);
        return false;
    }
    size_t used = 0;
    while (used + 1U < capacity) {
        size_t got = fread(storage + used, 1U, capacity - used - 1U, pipe);
        used += got;
        if (got == 0U) break;
    }
    storage[used] = '\0';
    int status = pclose(pipe);
#ifdef _WIN32
    if (status == -1 || status != 0 || used == 0U || used + 1U >= capacity) {
#else
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
        used == 0U || used + 1U >= capacity) {
#endif
        fputs("ylang: HTTP builds require libcurl development files and pkg-config (Fedora: libcurl-devel; Debian/Ubuntu: libcurl4-openssl-dev; macOS: brew install curl pkg-config; Windows MSYS2: mingw-w64-x86_64-curl).\n", stderr);
        return false;
    }
    *flag_count = 0U;
    for (char *part = strtok(storage, " \t\r\n");
         part != NULL; part = strtok(NULL, " \t\r\n")) {
        if (*flag_count >= flag_capacity) {
            fputs("ylang: too many libcurl compiler flags returned by pkg-config.\n", stderr);
            return false;
        }
        flags[(*flag_count)++] = part;
    }
    return *flag_count > 0U;
}

static int run_native_compiler(const char *cc, const char *c_path,
                               const char *output_path, bool uses_gui,
                               bool debug_mode, bool uses_http)
{
    const char *requested_opt = getenv("YLANG_OPT_LEVEL");
    const char *optimization = debug_mode ? "-O0" : "-O2";
    if (!debug_mode && requested_opt && requested_opt[0] != '\0') {
        if (strcmp(requested_opt, "0") == 0) optimization = "-O0";
        else if (strcmp(requested_opt, "1") == 0) optimization = "-O1";
        else if (strcmp(requested_opt, "2") == 0) optimization = "-O2";
        else if (strcmp(requested_opt, "3") == 0) optimization = "-O3";
        else if (strcmp(requested_opt, "s") == 0) optimization = "-Os";
        else {
            fprintf(stderr,
                    "ylang: invalid YLANG_OPT_LEVEL '%s' (use 0, 1, 2, 3, or s)\n",
                    requested_opt);
            return 64;
        }
    }

    char runtime_path[4096], include_path[4096];
    char flag_storage[4096], http_flag_storage[4096];
    char *sdl_flags[64], *http_flags[64];
    size_t sdl_flag_count = 0, http_flag_count = 0;
    if (uses_http &&
        !read_http_flags(http_flag_storage, sizeof(http_flag_storage),
                         http_flags, sizeof(http_flags) / sizeof(http_flags[0]),
                         &http_flag_count)) return 1;
    if (uses_gui) {
        if (!resolve_gui_paths(runtime_path, sizeof(runtime_path),
                               include_path, sizeof(include_path))) {
            fprintf(stderr,
                    "ylang: GUI runtime source or header not found. Run from the YLang source tree or install the optional GUI files with 'make install-gui'.\n");
            return 1;
        }
        if (!read_sdl2_flags(flag_storage, sizeof(flag_storage), sdl_flags,
                             sizeof(sdl_flags) / sizeof(sdl_flags[0]),
                             &sdl_flag_count)) return 1;
    }

    char *args[192];
    size_t n = 0;
    args[n++] = (char *)cc;
    if (uses_http) args[n++] = "-DYLANG_USE_HTTP=1";
    args[n++] = "-std=c17";
    args[n++] = "-Wall";
    args[n++] = "-Wextra";
    args[n++] = "-Wpedantic";
    args[n++] = "-Wno-unused-function";
    args[n++] = "-Wno-unused-variable";
    args[n++] = "-Wno-unused-but-set-variable";
    args[n++] = (char *)optimization;
    args[n++] = debug_mode ? "-g3" : "-g";
    if (debug_mode) args[n++] = "-fno-omit-frame-pointer";
#if !defined(__APPLE__) && !defined(_WIN32)
    args[n++] = "-D_FORTIFY_SOURCE=3";
#endif
    args[n++] = "-fstack-protector-strong";
#if !defined(_WIN32)
    args[n++] = "-fPIE";
#endif
    if (uses_gui) {
        args[n++] = "-I";
        args[n++] = include_path;
        args[n++] = (char *)c_path;
        args[n++] = runtime_path;
        args[n++] = "-lm";
        for (size_t i = 0; i < sdl_flag_count; i++) args[n++] = sdl_flags[i];
#if defined(__APPLE__)
        args[n++] = "-pie";
#elif !defined(_WIN32)
        args[n++] = "-pie";
        args[n++] = "-Wl,-z,relro,-z,now";
#endif
    } else {
        args[n++] = (char *)c_path;
        args[n++] = "-lm";
#if defined(__APPLE__)
        args[n++] = "-pie";
#elif !defined(_WIN32)
        args[n++] = "-pie";
        args[n++] = "-Wl,-z,relro,-z,now";
#endif
    }
    for (size_t i = 0; i < http_flag_count; i++) args[n++] = http_flags[i];
#ifdef _WIN32
    /* The generated runtime uses ShellExecuteA for image.open(). */
    args[n++] = "-lshell32";
#endif
    args[n++] = "-o";
    args[n++] = (char *)output_path;
    args[n] = NULL;

#ifdef _WIN32
    intptr_t status = _spawnvp(_P_WAIT, cc, (const char * const *)args);
    if (status == -1) {
        fprintf(stderr, "ylang: could not execute native compiler '%s': %s\n",
                cc, strerror(errno));
        return 1;
    }
    return (int)status;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "ylang: could not start native compiler: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        execvp(cc, args);
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

static int run_external(char *const argv[])
{
#ifdef _WIN32
    intptr_t status = _spawnvp(_P_WAIT, argv[0], (const char * const *)argv);
    if (status == -1) {
        fprintf(stderr, "ylang: cannot execute '%s': %s\n", argv[0], strerror(errno));
        return 1;
    }
    return (int)status;
#else
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "ylang: cannot start '%s': %s\n", argv[0], strerror(errno));
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "ylang: cannot execute '%s': %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    int status = 0;
    for (;;) {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) break;
        if (waited < 0 && errno == EINTR) continue;
        fprintf(stderr, "ylang: waitpid failed while running '%s': %s\n",
                argv[0], strerror(errno));
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}


static int emit_mir(Compiler *compiler)
{
    if (!compiler->program || compiler->program->function_count == 0) {
        fputs("ylang: emit-mir: the program contains no functions to lower.\n", stderr);
        return 1;
    }

    const size_t function_count = compiler->program->function_count;
    if (function_count > SIZE_MAX / sizeof(YIRFunction)) {
        fputs("ylang: emit-mir: too many functions to lower.\n", stderr);
        return 70;
    }

    YIRFunction *lowered = calloc(function_count, sizeof(*lowered));
    if (!lowered) {
        fputs("ylang: emit-mir: out of memory while preparing MIR output.\n", stderr);
        return 70;
    }

    size_t lowered_count = 0;
    for (size_t i = 0; i < function_count; ++i) {
        Function *function = compiler->program->functions[i];
        char error[256];
        const char *name = function && function->name ? function->name : "<anonymous>";

        if (!yir_lower_function(function, &lowered[i], error, sizeof(error))) {
            fprintf(stderr,
                    "ylang: emit-mir: cannot lower function '%s' at %zu:%zu: %s\n",
                    name,
                    function ? function->token.line : 0,
                    function ? function->token.column : 0,
                    error[0] ? error : "unknown lowering error");
            for (size_t j = 0; j < lowered_count; ++j)
                yir_destroy_function(&lowered[j]);
            free(lowered);
            return 1;
        }
        lowered_count++;

        if (!yir_verify_function(&lowered[i], error, sizeof(error))) {
            fprintf(stderr,
                    "ylang: emit-mir: verifier rejected function '%s': %s\n",
                    name, error[0] ? error : "unknown verification error");
            for (size_t j = 0; j < lowered_count; ++j)
                yir_destroy_function(&lowered[j]);
            free(lowered);
            return 1;
        }
    }

    char module_error[512];
    if (!yir_verify_module(lowered, function_count,
                           module_error, sizeof(module_error))) {
        fprintf(stderr, "ylang: emit-mir: module verifier rejected the program: %s\n",
                module_error[0] ? module_error : "unknown module verification error");
        for (size_t i = 0; i < lowered_count; ++i)
            yir_destroy_function(&lowered[i]);
        free(lowered);
        return 1;
    }

    for (size_t i = 0; i < function_count; ++i) {
        if (i != 0) (void)fputc('\n', stdout);
        yir_print_function(stdout, &lowered[i]);
    }

    int result = 0;
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fputs("ylang: emit-mir: failed while writing MIR to standard output.\n", stderr);
        result = 74;
    }

    for (size_t i = 0; i < lowered_count; ++i)
        yir_destroy_function(&lowered[i]);
    free(lowered);
    return result;
}


static void destroy_lowered_module(YIRFunction *functions, size_t count)
{
    if (!functions) return;
    for (size_t i = 0; i < count; ++i)
        yir_destroy_function(&functions[i]);
    free(functions);
}

static int emit_ir_c(Compiler *compiler, const char *output_path)
{
    if (!output_path) {
        fputs("ylang: emit-ir-c requires an output path with -o.\n", stderr);
        return 64;
    }
    if (!compiler->program || compiler->program->function_count == 0) {
        fputs("ylang: emit-ir-c: the program contains no functions to lower.\n", stderr);
        return 1;
    }

    const size_t function_count = compiler->program->function_count;
    if (function_count > SIZE_MAX / sizeof(YIRFunction)) {
        fputs("ylang: emit-ir-c: too many functions to lower.\n", stderr);
        return 70;
    }

    YIRFunction *lowered = calloc(function_count, sizeof(*lowered));
    if (!lowered) {
        fputs("ylang: emit-ir-c: out of memory while preparing IR.\n", stderr);
        return 70;
    }

    size_t lowered_count = 0;
    for (size_t i = 0; i < function_count; ++i) {
        Function *function = compiler->program->functions[i];
        char error[256];
        const char *name = function && function->name ?
                           function->name : "<anonymous>";
        if (!yir_lower_function(function, &lowered[i], error, sizeof(error))) {
            fprintf(stderr,
                "ylang: emit-ir-c: cannot lower function '%s' at %zu:%zu: %s\n",
                name,
                function ? function->token.line : 0,
                function ? function->token.column : 0,
                error[0] ? error : "unknown lowering error");
            destroy_lowered_module(lowered, lowered_count);
            return 1;
        }
        lowered_count++;
    }

    FILE *temporary = tmpfile();
    if (!temporary) {
        fprintf(stderr, "ylang: emit-ir-c: cannot create temporary output: %s\n",
                strerror(errno));
        destroy_lowered_module(lowered, lowered_count);
        return 73;
    }

    char error[512];
    if (!yir_emit_c_module(temporary, lowered, function_count,
                           error, sizeof(error))) {
        fprintf(stderr, "ylang: emit-ir-c: cannot emit verified IR as C: %s\n",
                error[0] ? error : "unknown C emission error");
        (void)fclose(temporary);
        destroy_lowered_module(lowered, lowered_count);
        return 1;
    }

    if (fseek(temporary, 0, SEEK_SET) != 0) {
        fprintf(stderr, "ylang: emit-ir-c: cannot rewind temporary C output: %s\n",
                strerror(errno));
        (void)fclose(temporary);
        destroy_lowered_module(lowered, lowered_count);
        return 74;
    }

    FILE *destination = fopen(output_path, "wb");
    if (!destination) {
        fprintf(stderr, "ylang: emit-ir-c: cannot open '%s': %s\n",
                output_path, strerror(errno));
        (void)fclose(temporary);
        destroy_lowered_module(lowered, lowered_count);
        return 73;
    }

    char buffer[8192];
    bool okay = true;
    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), temporary);
        if (count != 0 && fwrite(buffer, 1, count, destination) != count) {
            okay = false;
            break;
        }
        if (count < sizeof(buffer)) {
            if (ferror(temporary)) okay = false;
            break;
        }
    }
    if (fclose(destination) != 0) okay = false;
    if (fclose(temporary) != 0) okay = false;

    if (!okay) {
        fprintf(stderr, "ylang: emit-ir-c: failed while writing '%s'.\n",
                output_path);
        (void)remove(output_path);
        destroy_lowered_module(lowered, lowered_count);
        return 74;
    }

    printf("Generated IR C: %s\n", output_path);
    destroy_lowered_module(lowered, lowered_count);
    return 0;
}

int ylang_run(const char *command, const char *input_path,
              const char *output_path, const char *cc)
{
#ifdef _WIN32
    if (strcmp(command, "debug") == 0) {
        fputs("ylang: interactive debugging is not implemented on Windows yet. The native debug command currently supports GDB on Linux and LLDB on macOS.\n", stderr);
        return 64;
    }
#endif
    size_t source_length = 0;
    char *source = read_source_file(input_path, &source_length);
    if (!source) {
        fprintf(stderr, "ylang: cannot read '%s': %s\n", input_path, strerror(errno));
        return 66;
    }

    /*
     * The lexer uses NUL as its end-of-input sentinel. Reject embedded NUL
     * bytes before lexing so a binary source file cannot be accepted as only
     * its valid-looking prefix.
     */
    if (memchr(source, '\0', source_length) != NULL) {
        fprintf(stderr,
                "ylang: source file '%s' contains a NUL byte; source files must be text.\n",
                input_path);
        free(source);
        return 65;
    }

    if (output_path && paths_refer_same_file(input_path, output_path)) {
        fprintf(stderr,
                "ylang: input '%s' and output '%s' refer to the same file; refusing to overwrite source.\n",
                input_path, output_path);
        free(source);
        return 64;
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
    (void)load_program_modules(&compiler, input_path);
    if (!compiler.had_error) (void)check_program(&compiler);

    if (compiler.had_error) {
        if (strcmp(command, "check") == 0) {
            fprintf(stderr, "ylang: check failed with %u error(s).\n",
                    compiler.error_count);
        } else if (strcmp(command, "emit-c") == 0) {
            fprintf(stderr, "ylang: emit-c failed with %u error(s); no C source was generated.\n",
                    compiler.error_count);
        } else if (strcmp(command, "emit-mir") == 0) {
            fprintf(stderr, "ylang: emit-mir failed with %u error(s); no MIR was generated.\n",
                    compiler.error_count);
        } else if (strcmp(command, "emit-ir-c") == 0) {
            fprintf(stderr, "ylang: emit-ir-c failed with %u error(s); no C source was generated.\n",
                    compiler.error_count);
        } else if (strcmp(command, "emit-asm") == 0) {
            fprintf(stderr, "ylang: emit-asm failed with %u error(s); no assembly was generated.\n",
                    compiler.error_count);
        } else if (strcmp(command, "build-asm") == 0) {
            fprintf(stderr, "ylang: build-asm failed with %u error(s); no executable was produced.\n",
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

    if (strcmp(command, "emit-mir") == 0) {
        int result = emit_mir(&compiler);
        arena_destroy(&compiler.arena);
        free(source);
        return result;
    }

    if (strcmp(command, "emit-ir-c") == 0) {
        int result = emit_ir_c(&compiler, output_path);
        arena_destroy(&compiler.arena);
        free(source);
        return result;
    }

    if (strcmp(command, "build-asm") == 0) {
#if !defined(__linux__) || !defined(__x86_64__)
        fputs("ylang: build-asm currently supports Linux x86-64 only.\n", stderr);
        arena_destroy(&compiler.arena);
        free(source);
        return 64;
#else
        const char *target = output_path ? output_path : "a.out";
        if (paths_refer_same_file(input_path, target)) {
            fprintf(stderr,
                    "ylang: input '%s' and output '%s' refer to the same file; refusing to overwrite source.\n",
                    input_path, target);
            arena_destroy(&compiler.arena);
            free(source);
            return 64;
        }

        char *temporary_directory = create_temporary_directory("ylang-asm-build");
        if (!temporary_directory) {
            fprintf(stderr, "ylang: cannot create temporary assembly build directory: %s\n",
                    strerror(errno));
            free(temporary_directory);
            arena_destroy(&compiler.arena);
            free(source);
            return 73;
        }
        size_t asm_size = strlen(temporary_directory) + sizeof("/generated.asm");
        size_t object_size = strlen(temporary_directory) + sizeof("/generated.o");
        char *asm_path = malloc(asm_size);
        char *object_path = malloc(object_size);
        if (!asm_path || !object_path) {
            fputs("ylang: out of memory preparing NASM build.\n", stderr);
            free(asm_path);
            free(object_path);
            (void)rmdir(temporary_directory);
            free(temporary_directory);
            arena_destroy(&compiler.arena);
            free(source);
            return 70;
        }
        (void)snprintf(asm_path, asm_size, "%s/generated.asm", temporary_directory);
        (void)snprintf(object_path, object_size, "%s/generated.o", temporary_directory);

        int result = 1;
        if (generate_asm(&compiler, asm_path)) {
            char *nasm_args[] = {
                "nasm", "-f", "elf64", asm_path, "-o", object_path, NULL
            };
            result = run_external(nasm_args);
            if (result != 0) {
                fprintf(stderr, "ylang: NASM failed (exit %d).\n", result);
            } else {
                char *ld_args[] = { "ld", "-o", (char *)target, object_path, NULL };
                result = run_external(ld_args);
                if (result != 0) {
                    fprintf(stderr, "ylang: ld failed (exit %d).\n", result);
                }
            }
        }
        if (result == 0) printf("Build succeeded (NASM + ld): %s\n", target);

        (void)unlink(asm_path);
        (void)unlink(object_path);
        (void)rmdir(temporary_directory);
        free(asm_path);
        free(object_path);
        free(temporary_directory);
        arena_destroy(&compiler.arena);
        free(source);
        return result;
#endif
    }

    if (strcmp(command, "emit-asm") == 0) {
        const char *asm_path = output_path;
        if (!asm_path) {
            if (ylang_mkdir("build", 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "ylang: cannot create build directory: %s\n",
                        strerror(errno));
                arena_destroy(&compiler.arena);
                free(source);
                return 73;
            }
            asm_path = "build/ylang-generated.asm";
        }
        if (paths_refer_same_file(input_path, asm_path)) {
            fprintf(stderr,
                    "ylang: input '%s' and output '%s' refer to the same file; refusing to overwrite source.\n",
                    input_path, asm_path);
            arena_destroy(&compiler.arena);
            free(source);
            return 64;
        }
        bool okay = generate_asm(&compiler, asm_path);
        if (okay) printf("Generated NASM assembly: %s\n", asm_path);
        arena_destroy(&compiler.arena);
        free(source);
        return okay ? 0 : 1;
    }

    const bool emit_c_command = strcmp(command, "emit-c") == 0;
    const char *c_path = output_path;
    const char *native_target = output_path ? output_path : "a.out";
    if (!emit_c_command && paths_refer_same_file(input_path, native_target)) {
        fprintf(stderr,
                "ylang: input '%s' and output '%s' refer to the same file; refusing to overwrite source.\n",
                input_path, native_target);
        arena_destroy(&compiler.arena);
        free(source);
        return 64;
    }
    char *generated_path = NULL;
    char *temporary_directory = NULL;

    if (emit_c_command) {
        if (!c_path) {
            if (ylang_mkdir("build", 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "ylang: cannot create build directory: %s\n", strerror(errno));
                arena_destroy(&compiler.arena);
                free(source);
                return 73;
            }
            c_path = "build/ylang-generated.c";
        }
        if (paths_refer_same_file(input_path, c_path)) {
            fprintf(stderr,
                    "ylang: input '%s' and output '%s' refer to the same file; refusing to overwrite source.\n",
                    input_path, c_path);
            arena_destroy(&compiler.arena);
            free(source);
            return 64;
        }
    } else {
        char *template = create_temporary_directory("ylang-build");
        if (!template) {
            fprintf(stderr, "ylang: cannot create a temporary build directory: %s\n", strerror(errno));
            free(template);
            arena_destroy(&compiler.arena);
            free(source);
            return 73;
        }
        temporary_directory = template;
        size_t path_size = strlen(temporary_directory) + sizeof("/generated.c");
        generated_path = malloc(path_size);
        if (!generated_path) {
            fputs("ylang: out of memory while preparing native build\n", stderr);
            (void)rmdir(temporary_directory);
            free(temporary_directory);
            arena_destroy(&compiler.arena);
            free(source);
            return 70;
        }
        (void)snprintf(generated_path, path_size, "%s/generated.c", temporary_directory);
        c_path = generated_path;
    }

    if (!generate_c(&compiler, c_path)) {
        if (temporary_directory) {
            (void)unlink(c_path);
            (void)rmdir(temporary_directory);
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
        const char *target = native_target;
        bool debug_mode = strcmp(command, "debug") == 0;
        result = run_native_compiler(cc ? cc : "gcc", c_path, target,
                                     compiler.uses_gui, debug_mode, compiler.uses_http);
        if (result == 0) {
            printf("Build succeeded: %s\n", target);
            if (debug_mode) {
#ifdef _WIN32
                fputs("ylang: the interactive debug command is not yet supported on Windows; use a Linux GDB or macOS LLDB build.\n", stderr);
                result = 64;
#elif defined(__APPLE__)
                char *debugger_args[] = { "lldb", "--", (char *)target, NULL };
                result = run_external(debugger_args);
#else
                char *debugger_args[] = { "gdb", "-q", "--args", (char *)target, NULL };
                result = run_external(debugger_args);
#endif
            }
        } else {
            fprintf(stderr, "ylang: native compilation failed (exit %d).\n", result);
        }
        (void)unlink(c_path);
        (void)rmdir(temporary_directory);
    }

    free(generated_path);
    free(temporary_directory);
    arena_destroy(&compiler.arena);
    free(source);
    return result;
}

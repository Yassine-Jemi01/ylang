#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_module_context(Compiler *compiler, Module *module)
{
    compiler->filename = module->path;
    compiler->source = module->source;
    compiler->source_length = module->source_length;
}

static char *canonical_path(Compiler *compiler, const char *path)
{
    char *resolved = realpath(path, NULL);
    if (!resolved) return NULL;
    char *copy = arena_strndup(&compiler->arena, resolved, strlen(resolved));
    free(resolved);
    return copy;
}

static Module *find_module(Compiler *compiler, const char *canonical)
{
    for (size_t i = 0; i < compiler->module_count; i++) {
        Module *module = compiler->modules[i];
        if (module->canonical_path &&
            strcmp(module->canonical_path, canonical) == 0) return module;
    }
    return NULL;
}

static Module *new_module(Compiler *compiler, const char *path,
                          const char *canonical, char *source,
                          size_t source_length, bool is_root)
{
    Module *module = arena_alloc(&compiler->arena, sizeof(*module));
    module->path = arena_strndup(&compiler->arena, path, strlen(path));
    module->canonical_path = arena_strndup(&compiler->arena, canonical,
                                           strlen(canonical));
    module->source = source;
    module->source_length = source_length;
    module->id = compiler->module_count;
    module->is_root = is_root;
    compiler->modules = arena_vector_append(&compiler->arena,
        compiler->modules, compiler->module_count,
        sizeof(*compiler->modules), &module);
    compiler->module_count++;
    if (is_root) compiler->root_module = module;
    return module;
}

static char *resolve_import_path(Compiler *compiler, Module *from,
                                 const char *requested)
{
    if (requested[0] == '/') return canonical_path(compiler, requested);
    const char *base = from->canonical_path ? from->canonical_path : from->path;
    const char *slash = strrchr(base, '/');
    size_t directory_length;
    if (!slash) {
        base = ".";
        directory_length = 1;
    } else if (slash == base) {
        directory_length = 1;
    } else {
        directory_length = (size_t)(slash - base);
    }
    size_t requested_length = strlen(requested);
    if (directory_length > SIZE_MAX - requested_length - 2U) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    size_t size = directory_length + requested_length + 2U;
    char *joined = arena_alloc(&compiler->arena, size);
    if (directory_length == 1U && base[0] == '/') {
        (void)snprintf(joined, size, "/%s", requested);
    } else {
        (void)snprintf(joined, size, "%.*s/%s",
                       (int)directory_length, base, requested);
    }
    return canonical_path(compiler, joined);
}

static char *read_module_source(Compiler *compiler, const char *path,
                                size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        int saved = errno; fclose(file); errno = saved; return NULL;
    }
    long file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        int saved = errno ? errno : EIO;
        fclose(file); errno = saved; return NULL;
    }
    if ((uintmax_t)file_size >= (uintmax_t)SIZE_MAX) {
        fclose(file); errno = EFBIG; return NULL;
    }
    size_t size = (size_t)file_size;
    char *temporary = malloc(size + 1U);
    if (!temporary) { fclose(file); errno = ENOMEM; return NULL; }
    size_t got = fread(temporary, 1, size, file);
    bool okay = got == size && ferror(file) == 0;
    int saved = errno;
    fclose(file);
    if (!okay) {
        free(temporary); errno = saved ? saved : EIO; return NULL;
    }
    if (memchr(temporary, '\0', size) != NULL) {
        free(temporary); errno = EILSEQ; return NULL;
    }
    temporary[size] = '\0';
    char *source = arena_alloc(&compiler->arena, size + 1U);
    memcpy(source, temporary, size + 1U);
    free(temporary);
    *length = size;
    return source;
}

static Function *module_find_function(Module *module, const char *name)
{
    if (!module || !module->program) return NULL;
    for (size_t i = 0; i < module->program->function_count; i++) {
        Function *function = module->program->functions[i];
        if (strcmp(function->name, name) == 0) return function;
    }
    return NULL;
}

static VarDecl *module_find_global(Module *module, const char *name)
{
    if (!module || !module->program) return NULL;
    for (size_t i = 0; i < module->program->global_count; i++) {
        VarDecl *variable = module->program->globals[i];
        if (strcmp(variable->name, name) == 0) return variable;
    }
    return NULL;
}

static bool module_owns_name(Module *module, const char *name)
{
    return module_find_function(module, name) != NULL ||
           module_find_global(module, name) != NULL;
}

static bool imported_name_seen_before(Module *module, size_t import_index,
                                      size_t name_index, const char *name)
{
    for (size_t i = 0; i <= import_index; i++) {
        ImportDecl *import = module->program->imports[i];
        size_t limit = i == import_index ? name_index : import->name_count;
        for (size_t j = 0; j < limit; j++)
            if (strcmp(import->names[j]->name, name) == 0) return true;
    }
    return false;
}

static bool bind_imports(Compiler *compiler, Module *module)
{
    bool okay = true;
    set_module_context(compiler, module);
    for (size_t i = 0; i < module->program->import_count; i++) {
        ImportDecl *import = module->program->imports[i];
        Module *target = import->target;
        for (size_t j = 0; j < import->name_count; j++) {
            ImportName *name = import->names[j];
            if (imported_name_seen_before(module, i, j, name->name)) {
                diagnostic(compiler, name->token, "error", "E1025",
                           "This name is imported more than once into the same module.",
                           "Remove the duplicate import or use a distinct exported name.");
                okay = false;
                continue;
            }
            if (module_owns_name(module, name->name)) {
                diagnostic(compiler, name->token, "error", "E1025",
                           "An imported name conflicts with a function or global declared in this module.",
                           "Rename the local declaration or choose a different imported name.");
                okay = false;
                continue;
            }

            Function *function = module_find_function(target, name->name);
            VarDecl *variable = module_find_global(target, name->name);
            if (function && variable) {
                diagnostic(compiler, name->token, "error", "E1025",
                           "The imported name is ambiguous because the target module defines both a function and a global with that name.",
                           "Rename one of the declarations in the target module.");
                okay = false;
            } else if (function) {
                if (strcmp(function->name, "main") == 0) {
                    diagnostic(compiler, name->token, "error", "E1026",
                               "The program entry point 'main' cannot be imported.",
                               "Keep main() in the root source file.");
                    okay = false;
                } else {
                    name->function = function;
                }
            } else if (variable) {
                name->variable = variable;
            } else {
                diagnostic(compiler, name->token, "error", "E1024",
                           "The imported name was not found in the target module.",
                           "Check the spelling and make sure the target file declares this function or global.");
                okay = false;
            }
        }
    }
    return okay;
}

static bool load_module(Compiler *compiler, Module *module)
{
    module->loading = true;
    set_module_context(compiler, module);
    unsigned errors_before = compiler->error_count;
    module->program = parse_program(compiler);
    module->program->module = module;

    for (size_t i = 0; i < module->program->function_count; i++) {
        Function *function = module->program->functions[i];
        function->module = module;
        if (!module->is_root && strcmp(function->name, "main") == 0) {
            diagnostic(compiler, function->token, "error", "E1026",
                       "An imported module cannot define main().",
                       "Define main() only in the root program file.");
        }
        {
            /* Namespace every function, including root functions, so no source
               identifier can collide with a generated name from another module. */
            int needed = snprintf(NULL, 0, "yl_fn_m%zu_%s",
                                  module->id, function->name);
            if (needed < 0) {
                fputs("ylang: failed to create a module function name\n", stderr);
                module->loading = false;
                return false;
            }
            function->c_name = arena_alloc(&compiler->arena,
                                           (size_t)needed + 1U);
            (void)snprintf(function->c_name, (size_t)needed + 1U,
                           "yl_fn_m%zu_%s", module->id, function->name);
        }
    }
    for (size_t i = 0; i < module->program->global_count; i++)
        module->program->globals[i]->module = module;

    if (compiler->error_count != errors_before) {
        module->loading = false;
        return false;
    }

    bool okay = true;
    for (size_t i = 0; i < module->program->import_count; i++) {
        ImportDecl *import = module->program->imports[i];
        set_module_context(compiler, module);
        errno = 0;
        char *canonical = resolve_import_path(compiler, module, import->path);
        if (!canonical) {
            char suggestion[256];
            (void)snprintf(suggestion, sizeof(suggestion),
                           "Check that '%s' exists relative to '%s'.",
                           import->path, module->path);
            diagnostic(compiler, import->token, "error", "E1022",
                       "Cannot resolve the imported module file.", suggestion);
            okay = false;
            continue;
        }

        Module *target = find_module(compiler, canonical);
        if (target && target->loading) {
            diagnostic(compiler, import->token, "error", "E1023",
                       "Circular module imports are not supported.",
                       "Break the import cycle so module dependencies form a directed acyclic graph.");
            okay = false;
            continue;
        }
        if (!target) {
            size_t source_length = 0;
            char *source = read_module_source(compiler, canonical, &source_length);
            if (!source) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                               "Could not read '%s': %s", canonical,
                               strerror(errno));
                diagnostic(compiler, import->token, "error", "E1022",
                           "Cannot read the imported module source.", suggestion);
                okay = false;
                continue;
            }
            target = new_module(compiler, canonical, canonical, source,
                                source_length, false);
            if (!load_module(compiler, target)) okay = false;
            set_module_context(compiler, module);
        }
        import->target = target;
        if (!target->loaded) okay = false;
    }

    if (okay && !bind_imports(compiler, module)) okay = false;
    module->loading = false;
    module->loaded = okay;
    set_module_context(compiler, module);
    return okay;
}

static void append_program_items(Compiler *compiler, Program *merged,
                                 Program *part)
{
    for (size_t i = 0; i < part->global_count; i++) {
        VarDecl *item = part->globals[i];
        merged->globals = arena_vector_append(&compiler->arena, merged->globals,
            merged->global_count, sizeof(*merged->globals), &item);
        merged->global_count++;
    }
    for (size_t i = 0; i < part->function_count; i++) {
        Function *item = part->functions[i];
        merged->functions = arena_vector_append(&compiler->arena, merged->functions,
            merged->function_count, sizeof(*merged->functions), &item);
        merged->function_count++;
    }
    for (size_t i = 0; i < part->import_count; i++) {
        ImportDecl *item = part->imports[i];
        merged->imports = arena_vector_append(&compiler->arena, merged->imports,
            merged->import_count, sizeof(*merged->imports), &item);
        merged->import_count++;
    }
}

bool load_program_modules(Compiler *compiler, const char *input_path)
{
    char *canonical = canonical_path(compiler, input_path);
    if (!canonical) canonical = arena_strndup(&compiler->arena, input_path,
                                               strlen(input_path));
    Module *root = new_module(compiler, input_path, canonical, compiler->source,
                              compiler->source_length, true);
    bool okay = load_module(compiler, root);
    set_module_context(compiler, root);
    if (!okay || compiler->had_error) {
        compiler->program = root->program;
        return false;
    }

    Program *merged = arena_alloc(&compiler->arena, sizeof(*merged));
    merged->module = root;
    for (size_t i = 0; i < compiler->module_count; i++)
        append_program_items(compiler, merged, compiler->modules[i]->program);
    compiler->program = merged;
    return true;
}

#ifndef YLANG_INTERNAL_H
#define YLANG_INTERNAL_H

#include "ylang/compiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t used;
    size_t capacity;
    max_align_t aligner;
    unsigned char data[];
} ArenaBlock;

typedef struct {
    ArenaBlock *head;
} Arena;

struct Compiler {
    Arena arena;
    char *filename;
    char *source;
    size_t source_length;
    bool had_error;
    unsigned error_count;
    unsigned warning_count;
    Program *program;
    VarDecl **all_vars;
    size_t all_var_count;
    size_t next_var_id;
};

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

void *arena_alloc(Arena *arena, size_t size);
char *arena_strndup(Arena *arena, const char *text, size_t length);
void arena_destroy(Arena *arena);
/* Append to an arena-backed vector; items must originate from this helper. */
void *arena_vector_append(Arena *arena, void *items, size_t count,
                          size_t item_size, const void *item);

void sb_init(StringBuilder *sb);
void sb_append_n(StringBuilder *sb, const char *text, size_t length);
void sb_append(StringBuilder *sb, const char *text);
void sb_appendf(StringBuilder *sb, const char *format, ...);
void sb_destroy(StringBuilder *sb);

void diagnostic(Compiler *c, Token token, const char *level,
                const char *code, const char *message, const char *suggestion);
char *token_copy(Compiler *c, Token token);
const char *type_name(YType type);

Program *parse_program(Compiler *c);
bool check_program(Compiler *c);
Function *find_function(Compiler *c, const char *name);
bool generate_c(Compiler *c, const char *path);

#endif

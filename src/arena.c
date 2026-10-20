#include "internal.h"

#include <stdlib.h>
#include <string.h>

/* ----------------------------- arena memory ----------------------------- */
void *arena_alloc(Arena *arena, size_t size)
{
    const size_t alignment = _Alignof(max_align_t);
    if (size == 0) size = 1;
    ArenaBlock *block = arena->head;
    size_t offset = 0;
    if (block) {
        if (block->used > SIZE_MAX - (alignment - 1U)) {
            fputs("ylang: fatal: arena offset overflow\n", stderr);
            exit(70);
        }
        offset = (block->used + alignment - 1U) & ~(alignment - 1U);
    }

    if (!block || offset > block->capacity ||
        size > block->capacity - offset) {
        if (size > SIZE_MAX - alignment) {
            fputs("ylang: fatal: arena allocation size overflow\n", stderr);
            exit(70);
        }
        size_t capacity = size > 8192 ? size + alignment : 8192;
        if (capacity > SIZE_MAX - sizeof(*block)) {
            fputs("ylang: fatal: arena allocation size overflow\n", stderr);
            exit(70);
        }
        ArenaBlock *fresh = malloc(sizeof(*fresh) + capacity);
        if (!fresh) {
            fputs("ylang: fatal: out of memory\n", stderr);
            exit(70);
        }
        fresh->next = arena->head;
        fresh->used = 0;
        fresh->capacity = capacity;
        arena->head = fresh;
        block = fresh;
        offset = 0;
    }

    void *result = block->data + offset;
    block->used = offset + size;
    memset(result, 0, size);
    return result;
}

char *arena_strndup(Arena *arena, const char *text, size_t length)
{
    char *copy = arena_alloc(arena, length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void arena_destroy(Arena *arena)
{
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
}

typedef union {
    size_t capacity;
    max_align_t alignment;
} ArenaVectorHeader;

void *arena_vector_append(Arena *arena, void *items, size_t count,
                          size_t item_size, const void *item)
{
    if (!arena || item_size == 0 || !item || (count != 0 && !items)) {
        fputs("ylang: fatal: invalid vector state\n", stderr);
        exit(70);
    }

    ArenaVectorHeader *header = items ? ((ArenaVectorHeader *)items - 1) : NULL;
    size_t capacity = header ? header->capacity : 0;
    if (count == SIZE_MAX) {
        fputs("ylang: fatal: vector size overflow\n", stderr);
        exit(70);
    }

    if (count >= capacity) {
        size_t needed = count + 1;
        size_t next_capacity = capacity ? capacity : 8;
        while (next_capacity < needed) {
            if (next_capacity > SIZE_MAX / 2) {
                fputs("ylang: fatal: vector capacity overflow\n", stderr);
                exit(70);
            }
            next_capacity *= 2;
        }
        if (next_capacity > (SIZE_MAX - sizeof(*header)) / item_size) {
            fputs("ylang: fatal: vector allocation size overflow\n", stderr);
            exit(70);
        }

        header = arena_alloc(arena, sizeof(*header) + next_capacity * item_size);
        header->capacity = next_capacity;
        void *grown = header + 1;
        if (count > 0) memcpy(grown, items, count * item_size);
        items = grown;
    }

    memcpy((unsigned char *)items + count * item_size, item, item_size);
    return items;
}


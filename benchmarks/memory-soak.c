#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    void *ptr;
    size_t size;
    struct Node *next;
} Node;

int main(int argc, char **argv) {
    if (argc != 3) return 64;
    char *end = NULL;
    unsigned long long batch = strtoull(argv[1], &end, 10);
    if (!end || *end || batch == 0) return 64;
    int retain = strcmp(argv[2], "retain") == 0;
    if (!retain && strcmp(argv[2], "free") != 0) return 64;

    Node *head = NULL;
    unsigned long long rounds = 0;
    char command[32];
    fputs("READY\n", stdout);
    fflush(stdout);
    while (fgets(command, sizeof(command), stdin)) {
        char *command_end = NULL;
        long action = strtol(command, &command_end, 10);
        if (command_end == command || action == 0) break;
        for (unsigned long long i = 0; i < batch; i++) {
            char *data = malloc(257U);
            Node *node = malloc(sizeof(*node));
            if (!data || !node) return 70;
            memset(data, (int)('a' + (i % 26U)), 256U);
            data[256] = '\0';
            node->ptr = data;
            node->size = 257U;
            if (retain) {
                node->next = head;
                head = node;
            } else {
                free(data);
                free(node);
            }
        }
        rounds++;
        fputs("READY\n", stdout);
        fflush(stdout);
    }
    printf("%llu\n", rounds);
    while (head) {
        Node *next = head->next;
        free(head->ptr);
        free(head);
        head = next;
    }
    return 0;
}

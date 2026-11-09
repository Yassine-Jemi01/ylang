#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char *data;
    struct Node *next;
} Node;

int main(int argc, char **argv) {
    if (argc != 3) return 64;
    char *end = NULL;
    unsigned long long count = strtoull(argv[1], &end, 10);
    if (!end || *end || count == 0) return 64;
    int retain = strcmp(argv[2], "retain") == 0;
    if (!retain && strcmp(argv[2], "free") != 0) return 64;

    Node *head = NULL;
    uint64_t checksum = 0;
    for (unsigned long long i = 0; i < count; i++) {
        Node *node = malloc(sizeof(*node));
        char *data = malloc(257U);
        if (!node || !data) return 70;
        memset(data, (int)('a' + (i % 26U)), 256U);
        data[256] = '\0';
        checksum += (unsigned char)data[i % 256U];
        if (retain) {
            node->data = data;
            node->next = head;
            head = node;
        } else {
            free(data);
            free(node);
        }
    }
    printf("%llu %llu\n", count, (unsigned long long)checksum);
    while (head) {
        Node *next = head->next;
        free(head->data);
        free(head);
        head = next;
    }
    return 0;
}

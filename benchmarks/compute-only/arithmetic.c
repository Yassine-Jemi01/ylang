#include <stdint.h>

__attribute__((noinline))
int64_t benchmark(void) {
    int64_t i = 0;
    int64_t state = 1;
    int64_t total = 0;
    while (i < 10000000) {
        state = (state * 1664525 + 1013904223) % 2147483647;
        total += state;
        ++i;
    }
    return total;
}

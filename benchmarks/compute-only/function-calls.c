#include <stdint.h>

__attribute__((noinline)) static int64_t transform(int64_t value) {
    return (value * 31 + 7) % 1000003;
}

__attribute__((noinline))
int64_t benchmark(void) {
    int64_t i = 0;
    int64_t value = 1;
    int64_t total = 0;
    while (i < 10000000) {
        value = transform(value + i);
        total += value;
        ++i;
    }
    return total;
}

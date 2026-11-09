#include <stdint.h>
#include <stdio.h>
int main(void) {
    int64_t i = 0, state = 1, total = 0;
    while (i < 10000000) {
        state = (state * 1664525 + 1013904223) % 2147483647;
        total += state;
        ++i;
    }
    printf("%lld\n", (long long)total);
    return 0;
}

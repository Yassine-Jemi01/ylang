#include <stdint.h>
#include <stdio.h>
int main(void) {
    int64_t i = 0, state = 1, total = 0;
    while (i < 10000000) {
        state = (state * 1664525 + 1013904223) % 2147483647;
        if (state % 7 == 0) total += state;
        else total -= state % 97;
        ++i;
    }
    printf("%lld\n", (long long)total);
    return 0;
}

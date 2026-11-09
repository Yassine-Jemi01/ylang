#include <stdint.h>
#include <stdio.h>
static int64_t transform(int64_t value) {
    return (value * 31 + 7) % 1000003;
}
int main(void) {
    int64_t i = 0, value = 1, total = 0;
    while (i < 10000000) {
        value = transform(value + i);
        total += value;
        ++i;
    }
    printf("%lld\n", (long long)total);
    return 0;
}

#include <stdint.h>
#include <stdio.h>
int main(void) {
    const int64_t values[16] = {13,29,47,61,83,101,127,149,167,191,211,233,251,277,293,313};
    int64_t i = 0, index = 0, total = 0;
    while (i < 20000000) {
        total += values[index];
        ++index;
        if (index == 16) index = 0;
        ++i;
    }
    printf("%lld\n", (long long)total);
    return 0;
}

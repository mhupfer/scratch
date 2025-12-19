// read_ctr_el0.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

// ntoaarch64-gcc read_ctr_el0.c -o read_ctr_el0 -Wall -g


/* Read CTR_EL0 */
static inline uint64_t read_ctr_el0(void) {
    uint64_t val;
    asm volatile("mrs %0, ctr_el0" : "=r"(val));
    return val;
}

int main(void) {
    uint64_t ctr = read_ctr_el0();

    /* Extract bits 14-15: (ctr >> 14) & 0b11 */
    unsigned bits_14_15 = (unsigned)((ctr >> 14) & 0x3u);

    printf("CTR_EL0 = 0x%016" PRIx64 "\n", ctr);
    printf("L1Ip=%u\n", bits_14_15);
    printf("IDC=%lu\n", (ctr >> 28) & 1);
    printf("DIC=%lu\n", (ctr >> 29) & 1);

    return 0;
}

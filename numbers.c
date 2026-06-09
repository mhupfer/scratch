#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

int main(int argc, char const *argv[])
{
    int bits = atoi(argv[1]);
    const char *const strval = argv[2];

    if (bits == 64) {
        uint64_t u64 = strtoul(strval, NULL, 0);
        printf("%lu\n", u64);
        printf("%ld\n", (int64_t)u64);
    }

    return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <spawn.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char * argv[]) {

    posix_spawnattr_t   attr;
    int                 rc;

    posix_spawnattr_init(&attr);

    rc = posix_spawnattr_setrunmask64(&attr, ~0ULL);
    assert(rc == 0);

    uint64_t runmask;

    rc = posix_spawnattr_getrunmask64(&attr, &runmask);
    assert(rc == 0);
    assert(runmask == ~0ULL);
    printf("PASS\n");
}
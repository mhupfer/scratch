#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

unsigned int sync = 0;

int main(int argc, char *argv[]) {
    unsigned const count = atomic_fetch_or_explicit(&sync, 1, memory_order_relaxed);
    
    if (count == 0) {
        printf("super\n");
    }
}
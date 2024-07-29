#include <sys/neutrino.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

void print_el() {
    unsigned long level = ~0;
    asm("mrs x0, CurrentEL\n"
    :"=r" (level));
    printf("EL=%lu\n", level);
}

int main() {
    //print_el();
    if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)_NTO_IO_LEVEL_1 ) != -1) {
        print_el();
    } else {
        printf("ThreadCtl() failed: %s\n", strerror(errno));
    }
    InterruptDisable();
}
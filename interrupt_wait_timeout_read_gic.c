// #/********************************************************************#
// #                                                                     #
// #                       ██████╗ ███╗   ██╗██╗  ██╗                    #
// #                      ██╔═══██╗████╗  ██║╚██╗██╔╝                    #
// #                      ██║   ██║██╔██╗ ██║ ╚███╔╝                     #
// #                      ██║▄▄ ██║██║╚██╗██║ ██╔██╗                     #
// #                      ╚██████╔╝██║ ╚████║██╔╝ ██╗                    #
// #                       ╚══▀▀═╝ ╚═╝  ╚═══╝╚═╝  ╚═╝                    #
// #                                                                     #
// #/********************************************************************#

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <unistd.h>
#include <sys/mman.h>

// build: ntoaarch64-gcc interrupt_wait_timeout_read_gic.c -o interrupt_wait_timeout_read_gic -Wall -g


#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))



#include <stdint.h>

// Put in the base address of the Arm GICv2 of your SoC
#define QEMU_GICD_BASE       0x08000000UL       // GIC Distributor Base für QEMU virt
#define GICD_ISPENDR(n) (*(volatile uint32_t*)(distr_base + 0x200 + ((n) * 4)))
#define GICD_ISENABLER(n)     (*(volatile uint32_t*)(distr_base + 0x100 + ((n) * 4)))

off_t PHYS_GICD_BASE = QEMU_GICD_BASE;

/********************************/
/* dump_gic                     */
/********************************/
int dump_gic(uint32_t irq_id) {
    static void *distr_base = NULL;

    if (distr_base == NULL) {
        printf("PHYS_GICD_BASE=0x%lx\n", PHYS_GICD_BASE);
        distr_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, PHYS_GICD_BASE);
    }

    if (distr_base == MAP_FAILED) {
        failed(mmap, errno);
        return -1;
    }

    uint32_t reg_index = irq_id / 32;
    uint32_t bit_pos = irq_id % 32;

    uint32_t enabled = GICD_ISENABLER(reg_index) & (1 << bit_pos);
    uint32_t pending = GICD_ISPENDR(reg_index) & (1 << bit_pos);

    printf("enabled=%u pending=%u\n", enabled, pending);

    return 0;
}


/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    struct sigevent ev;

    if (argv[1]) {
        if (strncmp(argv[1], "GICD_BASE=", 10) == 0) {
            PHYS_GICD_BASE = strtoul(&argv[1][10], NULL, 0);
        }
    }

    SIGEV_INTR_INIT(&ev);

    int intr_id = InterruptAttachEvent(42, &ev, _NTO_INTR_FLAGS_TRK_MSK);
    if (intr_id == -1) {
        failed(InterruptAttachEvent, errno);
        return EXIT_FAILURE;
    }

    uint64_t to_ns = 100 *1000000UL;

    while (true)
    {
        if (TimerTimeout( CLOCK_MONOTONIC, _NTO_TIMEOUT_INTR, NULL, &to_ns, NULL) == -1) {
            failed(TimerTimeout, errno);
            break;
        }

        int res = InterruptWait(0, NULL);
        
        if (res == -1 && errno == ETIMEDOUT) {
            dump_gic(42);
        } else {
            //do something
            InterruptUnmask(42, intr_id);
        }
    }
    
    return EXIT_SUCCESS;
}

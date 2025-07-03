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
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
// #include <sys/trace.h>
// #include <spawn.h>

// build: ntoaarch64-gcc test_unexepected_interrupt.c -o test_unexepected_interrupt -Wall -g -I ~/mainline/stage/nto/usr/include -L ~/mainline/stage/nto/aarch64le/lib/


#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define mssleep(t) usleep((t * 1000UL))


#if 0
#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
        exit(1);                                                               \
    }
#endif

#include <stdint.h>

#define GICD_BASE       0x08000000UL       // GIC Distributor Base für QEMU virt
#define GICD_ISPENDR(n) (*(volatile uint32_t*)(distr_base + 0x200 + ((n) * 4)))
#define GICD_ISENABLER(n)     (*(volatile uint32_t*)(distr_base + 0x100 + ((n) * 4)))

/********************************/
/* trigger_interrupt            */
/********************************/
int trigger_interrupt(uint32_t irq_id) {
    static void *distr_base = NULL;

    if (distr_base == NULL) {
        distr_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, GICD_BASE);

        long level = _NTO_IO_LEVEL_2;

        if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)level) == -1) {
            failed(ThreadCtl, errno);
            return -1;
        }
    }

    if (distr_base == MAP_FAILED) {
        failed(mmap, errno);
        return -1;
    }

    uint32_t reg_index = irq_id / 32;
    uint32_t bit_pos = irq_id % 32;

    GICD_ISENABLER(reg_index) = (1 << bit_pos);
    GICD_ISPENDR(reg_index) = (1 << bit_pos);

    return 0;
}


/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    // printf(PASSED"Test PASS"ENDC"\n");
    struct sigevent ev;

    int chid = ChannelCreate(0);

    if (chid == -1) {
        failed(ChannelCreate, errno);
        return 1;
    }

    int coid = ConnectAttach(0, 0, chid, 0, 0);

    if (coid == -1) {
        failed(ConnectAttach, errno);
        return -1;
    }

    SIGEV_PULSE_INIT(&ev, coid, -1, 17, 0);

    if (MsgRegisterEvent(&ev, _NTO_REGEVENT_ALLOW_SELF) == -1) {
        failed(MsgRegisterEvent, errno);
        return EXIT_FAILURE;
    }

    if (InterruptAttachEvent(0x42424242U, &ev, 0) == -1) {
        failed(InterruptAttachEvent, errno);
        return EXIT_FAILURE;
    }

    trigger_interrupt(42);
    trigger_interrupt(42);
    trigger_interrupt(69);

    while (true) {
        struct _pulse p;

        if (MsgReceivePulse(chid, &p, sizeof(p), NULL) == 0) {
            struct interrupt_query_data data;
            int res = InterruptQuery(_NTO_INTR_QUERY_INEXPECTED_INTR, -1, 0,
                                     &data);

            if (res == EOK) {
                printf("Source %d reports %u unexepected interrupts\n",
                       data.vector, data.data.source.unexepected_intrs);
            } else {
                //potentially another thread consumed the 
                //unexepected interrupts
                if (errno != ESRCH) {
                    failed(MsgReceivePulse, errno);
                    return EXIT_FAILURE;
                }
            }
        } else {
            failed(MsgReceivePulse, errno);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

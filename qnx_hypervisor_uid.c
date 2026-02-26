//#/********************************************************************#
//#                                                                     #
//#                       ██████╗ ███╗   ██╗██╗  ██╗                    #
//#                      ██╔═══██╗████╗  ██║╚██╗██╔╝                    #
//#                      ██║   ██║██╔██╗ ██║ ╚███╔╝                     #
//#                      ██║▄▄ ██║██║╚██╗██║ ██╔██╗                     #
//#                      ╚██████╔╝██║ ╚████║██╔╝ ██╗                    #
//#                       ╚══▀▀═╝ ╚═╝  ╚═══╝╚═╝  ╚═╝                    #
//#                                                                     #
//#/********************************************************************#

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/neutrino.h>
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>

//build: ntoaarch64-gcc qnx_hypervisor_uid.c -o qnx_hypervisor_uid -Wall -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define mssleep(t) usleep((t*1000UL))

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

/**
 * Structure to hold HVC call results (x0-x3)
 */
typedef struct {
    uint32_t x0;
    uint32_t x1;
    uint32_t x2;
    uint32_t x3;
} hvc_result_t ;//__attribute__((packed));

#define SMCCC_VENDOR_HYP_CALL_UID 0x8600FF01
#define QNX_HYP_SMCCC_UID {0x65, 0x26, 0x0d, 0xb1, 0x80, 0x58, 0x47, 0x4b, 0xb9, 0x52, 0x39, 0x73, 0xfb, 0x80, 0x62, 0xc7}

uint8_t qnxhyp_uuid[16] = QNX_HYP_SMCCC_UID;

/**
 * Issue an HVC (Hypervisor Call) instruction with SMCCC calling convention
 *
 * @param fid Function identifier (SMCCC function ID)
 * @param arg1 First argument
 * @param arg2 Second argument
 * @param arg3 Third argument
 * @param result Pointer to structure to store the results (x0-x3)
 */
static inline void
hvc_call(uint64_t fid, uint64_t arg1, uint64_t arg2, uint64_t arg3,
         hvc_result_t *result) {
    register uint64_t x0 asm("x0") = fid;
    register uint64_t x1 asm("x1") = arg1;
    register uint64_t x2 asm("x2") = arg2;
    register uint64_t x3 asm("x3") = arg3;

    asm volatile("hvc #0\n"
                 : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
                 :
                 : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
                   "x13", "x14", "x15", "x16", "x17", "memory");

    // Store lower 32 bits of each return register
    result->x0 = (uint32_t)x0;
    result->x1 = (uint32_t)x1;
    result->x2 = (uint32_t)x2;
    result->x3 = (uint32_t)x3;
}


/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    long level = _NTO_IO_LEVEL_2;

    if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)level) == -1) {
        failed("ThreadCtl", errno);
        return EXIT_FAILURE;
    }

    hvc_result_t res;
    hvc_call(SMCCC_VENDOR_HYP_CALL_UID, 0, 0, 0, &res);

    if (res.x0 == ~0U) {
        test_failed;
    } else {
        uint8_t qnxhyp_uuid[16] = QNX_HYP_SMCCC_UID;

        if (memcmp(&res, qnxhyp_uuid, sizeof(qnxhyp_uuid))) {
            test_failed;
        }
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
#include <sys/memmsg.h>
#include <sys/procmsg.h>

// build: qcc test_exposed_kernel_stack.c -o test_exposed_kernel_stack -Wall -g

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define usleep_ms(t) usleep((t * 1000UL))

#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
        exit(1);                                                               \
    }

pid64_t *get_all_pids();
static long get_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr,
                        uintptr_t endaddr, mem_map_info_t *buffer,
                        size_t bsize);

/********************************/
/* main							*/
/********************************/
int
main(int argn, char *argv[]) {
    pid64_t *pidlist = get_all_pids();

    if (pidlist) {
        /* procmgr_get_all_pid use a cache on the stack */
        mem_map_info_t mapinfo[256];

        long mappings =
            get_mapinfo(_MEM_MAP_INFO_REGION, 0, 0x1000, 0x0000007fffffffffUL,
                        mapinfo, sizeof(mapinfo));

        if (mappings > 0) {
            mappings /= sizeof(mapinfo[0]);

            for (unsigned u = 0; u < mappings; u++) {
                if (mapinfo[u].o.paddr != 0) {
                    printf("virtual=%lx size=%lx physical=%lx\n",
                           mapinfo[u].o.vaddr, mapinfo[u].o.size,
                           mapinfo[u].o.paddr);
                    test_failed;
                }
            }

        } else {
            failed(get_mapinfo, -mappings);
        }

        free(pidlist);
    } else {
        failed(get_all_pids, errno);
    }
    printf(PASSED "Test PASS" ENDC "\n");
    return EXIT_SUCCESS;
}

/********************************/
/* get_all_pids                 */
/********************************/
pid64_t *
get_all_pids() {

    /*------------ _PROC_GETPID64_GET_ALL ---------------*/

    proc_getallpid_t msg;
    int pidlist_size = 128, rc;
    pid64_t *pidlist;

    msg.i.type = _PROC_GET_ALL_PID;

    pidlist = malloc(pidlist_size);

    while (pidlist) {
        rc = MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), pidlist,
                       pidlist_size);

        if (rc > 0) {
            if (rc > pidlist_size) {
                free(pidlist);
                pidlist_size = rc;
                pidlist = malloc(pidlist_size);
            } else {
                return pidlist;
            }
        } else {
            failed(MsgSend_r, -rc);
            errno = -rc;
            break;
        }
    }

    return NULL;
}

/********************************/
/* get_mapinfo      			*/
/********************************/
static long
get_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr,
            uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize) {
    memset(buffer, 0, sizeof(buffer->i));
    buffer->i.type = _MEM_MAP_INFO;
    buffer->i.subtype = submsg;
    buffer->i.pid = pid64;
    buffer->i.startaddr = startaddr;
    buffer->i.endaddr = endaddr;

    return MsgSend_r(MEMMGR_COID, buffer, sizeof(buffer->i), (void *)buffer,
                     bsize);
}

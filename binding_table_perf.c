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
// #include <pthread.h>
#include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
// #include <semaphore.h>

//build: qcc binding_table_perf.c -o binding_table_perf -Wall -g -L ~/mainline/stage/nto/x86_64/lib/
//build: ntoaarch64-gcc  binding_table_perf.c -o binding_table_perf_aarch64 -Wall -g -L ~/mainline/stage/nto/aarch64le/lib/


#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 0
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

#define DEV_NAME "/dev/binding_table_perf"
// #define SEM_NAME "sem_binding_table_perf"


int	server_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra) {
    return EOK;
}


/********************************/
/* server_read                  */
/********************************/
int	server_read(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    printf("Server exit\n");
    exit(0);
}

unsigned closed_fd_count = 0;

/********************************/
/* server_close_ocb             */
/********************************/
int	server_close_ocb(resmgr_context_t *ctp, void *reserved, RESMGR_OCB_T *ocb) {
    closed_fd_count++;
    return iofunc_close_ocb_default(ctp, reserved, ocb);
}

#define NUMBER_OF_FDS 20000
extern uint32_t  _resmgr_lock_binding_by_scoid_avg_time_us;
extern uint32_t  _resmgr_lock_binding_by_scoid_us_total;
extern uint32_t  _resmgr_add_binding_avg_time_us;
extern uint32_t  _resmgr_add_binding_us_total;
extern uint32_t  remove_binding_avg_time_us;
extern uint32_t  remove_binding_us_total;

void child();

#define TS_TO_MS(ts) ((ts).tv_sec * 1000 + (ts).tv_nsec / 1000000)

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    int no_of_childs = 1;

    if (argc > 1) {
        no_of_childs = atoi(argv[1]);
    }

    for (int i = 0; i < no_of_childs; i++) {
        int cpid = fork();

        switch (cpid)
        {
        case -1:
            failed(fork, errno);
            return EXIT_FAILURE;
            break;
        case 0:
            /* child -> no return*/
            child();
            break;
        default: {
            break;
            }
        }
    }

    iofunc_attr_t attr;
    iofunc_attr_init(&attr, S_IRUSR  | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, NULL, NULL);
    attr.uid = getuid();
    attr.gid = getgid();

    int chid = ChannelCreate(_NTO_CHF_DISCONNECT);

    if (chid == -1) {
        failed(ChannelCreate, errno);
    	return EXIT_FAILURE;
    }

    dispatch_t *dpp;
    dpp = dispatch_create_channel(chid, DISPATCH_FLAG_NOLOCK);

    if (dpp == NULL) {
        failed(dispatch_create_channel, errno);
    	return EXIT_FAILURE;
    }

    resmgr_connect_funcs_t      connect_funcs;
    resmgr_io_funcs_t           io_funcs;

    iofunc_func_init (_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                      _RESMGR_IO_NFUNCS, &io_funcs);

    io_funcs.read = server_read;
    io_funcs.close_ocb = server_close_ocb;

    resmgr_attr_t   rattr;
    memset (&rattr, 0, sizeof (rattr));

    int id1 = resmgr_attach(dpp, &rattr, DEV_NAME, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id1 == -1 )
    {
    	failed("resmgr_attach", errno);
    	return EXIT_FAILURE;
    }

    dispatch_context_t *ctp = dispatch_context_alloc(dpp);

    if (ctp == NULL)
    {
    	failed("dispatch_context_alloc", errno);
    	return -1;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    while (dispatch_block(ctp))
    {
        dispatch_handler(ctp);

        if (closed_fd_count >= NUMBER_OF_FDS * no_of_childs) {
            break;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("Total test duration %lu ms\n", TS_TO_MS(t1) - TS_TO_MS(t0));
    printf("Total open/close cycles %u\n", closed_fd_count);
    printf("Average binding table add time is %u µs total time %u ms\n",
        _resmgr_add_binding_avg_time_us,
        _resmgr_add_binding_us_total / 1000);
    printf("Average binding table remove time is %u µs total time %u ms\n",
        remove_binding_avg_time_us,
        remove_binding_us_total / 1000);
    printf("Average scoid lookup time is %u µs total time %u ms\n",
           _resmgr_lock_binding_by_scoid_avg_time_us,
           _resmgr_lock_binding_by_scoid_us_total / 1000);

    // sem_close(sem);
    return EXIT_SUCCESS;
}


/********************************/
/* child                        */
/********************************/
void child() {
    usleep_ms(1);
        // printf("child open \n");
nochmal:
    int fd = open(DEV_NAME, O_RDONLY);

    if (fd == -1) {
        if (errno == ENOENT) {
            usleep_ms(1);
            goto nochmal;
        } else {
            failed(open, errno);
            exit(1);
        }
    }

    close(fd);
    struct rlimit rl;
    rl.rlim_max = rl.rlim_cur = NUMBER_OF_FDS + 100;

    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        failed(setrlimit, errno);
        exit(1);
    }

    for (unsigned u = 0; u < NUMBER_OF_FDS - 1; u++) {
// once_again:
        fd = open(DEV_NAME, O_RDONLY);

        if (fd == -1) {
            // if (errno == ENFILE) {
            //     usleep_ms(1);
            //     goto once_again;
            // } else {
                failed(open, errno);
                break;
            // }
        }

        if (close(fd) == -1) {
            failed(close, errno);
        }
    }


    exit(0);

}
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

//build: qcc open_ConnectDetach.c -o open_ConnectDetach -Wall -g -L ~/mainline/stage/nto/x86_64/lib/


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

#define THE_NAME "/dev/einname"


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
    return EOK;
}

#define NUMBER_OF_FDS 6000
extern uint32_t  _resmgr_lock_binding_by_scoid_avg_time_us;
extern uint32_t  _resmgr_lock_binding_by_scoid_us_total;

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    // printf(PASSED"Test PASS"ENDC"\n");

    // int chid = ChannelCreate(_NTO_CHF_DISCONNECT);

    // if (chid == -1) {
    //     failed(ChannelCreate, errno);
    //     return EXIT_FAILURE;
    // }


    // printf("server chid %d\n", chid);

    // int ppid = getpid();
    int cpid = fork();

    switch (cpid)
    {
    case -1:
        failed(fork, errno);
        return EXIT_FAILURE;
        break;
    case 0:
        /* child */
        usleep_ms(1);
        // printf("child open \n");
nochmal:
        int fd = open(THE_NAME, O_RDONLY);

        if (fd == -1) {
            failed(open, errno);
            usleep_ms(1);
            goto nochmal;
            // return EXIT_FAILURE;
        }


        ConnectDetach(fd);
        close(fd);

        fd = open(THE_NAME, O_RDONLY);

        if (fd == -1) {
            failed(open2, errno);
            // return EXIT_FAILURE;
        }
        

        char buf[8];

        if (read(fd, buf, sizeof buf) == -1) {
            failed(read, errno);
        }

        /* sleep longer as the server */
        // usleep_ms(1000);
        // *((uint64_t*)7) = 0;
        exit(0);
        break;
    default: {
        break;
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

    // connect_funcs.open = server_open;
    io_funcs.read = server_read;
    io_funcs.close_ocb = server_close_ocb;

    resmgr_attr_t   rattr;
    memset (&rattr, 0, sizeof (rattr));

    /* first attach: attach DEFAULT_PATH non exclusive */
    int id1 = resmgr_attach(dpp, &rattr, THE_NAME, _FTYPE_ANY, 0,
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


    while (dispatch_block(ctp))
    {
        dispatch_handler(ctp);

        if (closed_fd_count >= NUMBER_OF_FDS) {
            break;
        }
    }

    printf("Average scoid lookup time is %u µs total time %u ms\n", _resmgr_lock_binding_by_scoid_avg_time_us, _resmgr_lock_binding_by_scoid_us_total / 1000);
    
    return EXIT_SUCCESS;
}

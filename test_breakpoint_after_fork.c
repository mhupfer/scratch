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
// #include <sys/resource.h>
// #include <sys/mman.h>
#include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
#include <sys/procfs.h>
#include <sys/trace.h>

//build: qcc test_breakpoint_after_fork.c -o test_breakpoint_after_fork_x86_64 -Wall -g

/** 
 * @brief Checks if breakpint can be set in a child process.
 * The test program aka debugger creates a child and stops it via debug interface. 
 * Register for the fork event.
 * Let the child continue. The child forks a grand child. The debugger should
 * receive the fork event. Set a breakpoint in the grand child , continue the
 * grand child and check if the breakpoint event for the grand child is received.
 * ASLR muts be turned off.
 * 
 * */

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))
int continue_proc(int fd);
int stop_proc(int fd);
int install_notification(int fd, int *chid, int offs);
int get_proc_status(int fd, procfs_status *status);
int recv_pulse_to(int chid, struct _pulse *pulse, unsigned to_ms);
int set_break(int fd, void *addr);

int debugger();
int child();
int grand_child();
void grand_child_func();

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {

    int cpid = fork();

    switch (cpid)
    {
    case -1:
        failed(fork, errno);
        return 1;
        break;
    case 0: 
        return child();
        break;
    default:
        if  (debugger(cpid) == EOK) {
            printf(PASSED"Test PASS"ENDC"\n");
        } else {
            printf(FAILED"Test Failed"ENDC"\n");
        }
        waitpid(cpid, 0, 0);
        break;
    }

    return EXIT_SUCCESS;
}


/********************************/
/* debugger                     */
/********************************/
int debugger(int cpid) {
    char path[64];

    // printf("debuggee pid %d debugger %d\n", debuggee, getpid());
    // fflush(stdout);

    sprintf(path, "/proc/%d/as", cpid);

    int fd = open(path, O_RDWR);

    if (fd == -1) {
        failed(open, errno);
        return -1;
    }

    //stop the child
    trace_logf(42, "%s", "dbg: stop child");
    if (stop_proc(fd) == -1) {
        failed(stop_proc, errno);
        close(fd);
        return -1;
    }
    
    int chid;

    // register event
    if (install_notification(fd, &chid, 0) == -1) {
        failed(install_notification, errno);
        close(fd);
        return -1;
    }

    //let child continue
    trace_logf(42, "%s", "dbg: cont child");

    if (continue_proc(fd) == -1) {
        failed(continue_proc, errno);
        close(fd);
        return -1;
    }

    struct _pulse   pulse;
    procfs_status   status = {0};
    int             res = EOK;

    /* loop until fork message */
    trace_logf(42, "%s", "dbg: wait 4 grand child fork");

    while(res == EOK && status.why != _DEBUG_WHY_CHILD) {
        res = recv_pulse_to(chid, &pulse, 1000);

        if (res == EOK) {
            res = get_proc_status(fd, &status);

            if (res != EOK) {
                failed(get_proc_status, errno);
            }
        } else {
            printf("Didn't receive fork notification\n");
        }
    }

    int gcfd;

    if (res == EOK) {
        //status.what == grand child pid
        sprintf(path, "/proc/%d/as", status.what);

        gcfd = open(path, O_RDWR);

        if (gcfd == -1) {
            failed(open, errno);
            res = -1;
        }
    }

    if (res == EOK) {
        if (install_notification(gcfd, &chid, 1) == -1) {
            failed(install_notification, errno);
            res = -1;
        }
    }

    if (res == EOK) {
        /* set a breakpoint in the grand child. Assumption: ASLR ifs off */
        res = set_break(gcfd, grand_child_func);

        if (res != EOK) {
            failed(set_break, errno);
        }
    }

    if (gcfd != -1) {
        //let grand child continue
        trace_logf(42, "%s", "dbg: cont grand child");

        if (continue_proc(gcfd) == -1) {
            failed(continue_proc, errno);
            res = -1;
        }
    }

    /* loop until break notification */
    status.why = 0;

    if (res == EOK)
        trace_logf(42, "%s", "dbg: wait 4 grand child breakpoint");

    while(res == EOK && status.why != _DEBUG_WHY_FAULTED) {
        int res = recv_pulse_to(chid, &pulse, 1000);
        if (res == EOK) {
            if (pulse.code != (_PULSE_CODE_MINAVAIL + 1))
                continue;

            res = get_proc_status(fd, &status);

            if (res != EOK) {
                failed(get_proc_status, errno);
            }
        } else {
            printf("Didn't receive fork notification");
        }
    }

    if (gcfd != -1) {
        //let grand child continue
        if (continue_proc(gcfd) == -1) {
            failed(continue_proc, errno);
            res = -1;
        }
        close(gcfd);
    }

    close(fd);
    return res;
}


/********************************/
/* child                        */
/********************************/
int child() {
    trace_logf(42, "%s", "child: enter ");
    // give the debugger time to attach
    usleep_ms(500);

    trace_logf(42, "%s", "child: fork grand child child");
    int gcpid = fork();

    switch (gcpid)
    {
    case -1:
        /* code */
        failed(fork, errno);
        return -1;
        break;
    case 0:
        return grand_child();
    default:
        waitpid(gcpid, 0, 0);
        break;
    }

    return EOK;
}


/********************************/
/* grand_child_func             */
/********************************/
void __attribute__ ((noinline)) grand_child_func() {
    usleep_ms(8);
}


/********************************/
/* grand_child                  */
/********************************/
int grand_child() {
    trace_logf(42, "%s", "gchild: entered");
    grand_child_func();
    return 0;
}

/********************************/
/* stop_proc                    */
/********************************/
int stop_proc(int fd) {
    debug_thread_t	status;

    int res = devctl(fd, DCMD_PROC_STOP, &status, sizeof status, 0);

    if (res != 0) {
        errno = res;
    	printf("devctl(PROC_STOP) failed: %s", strerror(res));
    }
    return res == 0? res : -1;
}

/********************************/
/* continue_proc                */
/********************************/
int continue_proc(int fd) {
    procfs_run		run;

    memset( &run, 0, sizeof(run) );
	run.flags |= _DEBUG_RUN_ARM;
	run.flags |= _DEBUG_RUN_CLRFLT | _DEBUG_RUN_CLRSIG;

    int res = devctl(fd, DCMD_PROC_RUN, &run, sizeof(run), 0);
	if (res != 0) {
        errno = res;
		printf("devctl(DCMD_PROC_RUN) failed: %s\n", strerror(res));
    }
    return res == 0? res : -1;
}


/********************************/
/* install_notification         */
/********************************/
int install_notification(int fd, int *chid, int offs) {
    struct sigevent     event;

    *chid = ChannelCreate(_NTO_CHF_DISCONNECT);

    if (*chid == -1) {
        failed(ChannelCreate, errno);
        return -1;
    }

    SIGEV_PULSE_INIT(&event, *chid, SIGEV_PULSE_PRIO_INHERIT, _PULSE_CODE_MINAVAIL + offs, 42);

    int res = EOK;

    if (MsgRegisterEvent(&event, fd) == 0) {
        res = devctl( fd, DCMD_PROC_EVENT, &event, sizeof(event), NULL );

        if (res != EOK) {
		    printf("devctl(DCMD_PROC_EVENT) failed: %s\n", strerror(res));
            res = -1;
        }
    } else {
        printf("MsgRegisterEvent() failed: %s\n", strerror(res));
        res = -1;
    }

    return res;
}


/********************************/
/* recv_pulse_to                */
/********************************/
int recv_pulse_to(int chid, struct _pulse *pulse, unsigned to_ms) {
    uint64_t to_ns = to_ms *1000000UL;

    if (TimerTimeout( CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &to_ns, NULL) == -1) {
        failed(TimerTimeout, errno);
        return -1;
    }

    return MsgReceivePulse(chid, pulse, sizeof(*pulse), NULL);
}


/********************************/
/* get_proc_status              */
/********************************/
int get_proc_status(int fd, procfs_status *status) {
    int res = devctl(fd, DCMD_PROC_STATUS, status, sizeof(procfs_status), 0);

	if (res != 0) {
        errno = res;
		printf("devctl(DCMD_PROC_STATUS) failed: %s\n", strerror(res));
    }

    return res == 0? res : -1;
}


/********************************/
/* set_break                    */
/********************************/
int set_break(int fd, void *addr) {
    debug_break_t brk;
    brk.type = _DEBUG_BREAK_EXEC;
    brk.size = 0;
    brk.addr = (uintptr64_t)addr;

    int res = devctl(fd, DCMD_PROC_BREAK, &brk, sizeof brk, 0);
    if (res != 0) {
        errno = res;
        printf("devctl(DCMD_PROC_BREAK) failed: %s\n", strerror(res));
        res = -1;
    }

    return res;
}
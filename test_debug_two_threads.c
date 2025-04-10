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
#include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
#include <sys/procfs.h>
//#include <sys/trace.h>
#include <spawn.h>
#include <sys/siginfo.h>
//build: 
// ntoaarch64-gcc test_debug_two_threads.c -o test_debug_two_threads_aarch64 -g -Wall
// ntox86_64-gcc test_debug_two_threads.c -o test_debug_two_threads_x86_64 -g -Wall 
 

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
}
#endif

int debugger(int *pcpid, char* argv[]);
int debuggee();

void init_run(procfs_run *prun);
int register_event(int fd, struct sigevent *pevent);
int devcl_set_breakpoint(const int fd, const uintptr_t addr);
int devcl_rm_breakpoint(const int fd, const uintptr_t addr);
int devctl_get_proc_info(const int fd, procfs_info *const info);

int devctl_run_process(const int fd, procfs_run * run);
int devctl_get_proc_status(const int fd, procfs_status *const status);
void breakpoint(uint32_t u);
int set_break_and_wait(int cpid, int fd, uintptr_t brk_addr, procfs_run *prun, sigset_t *pset);
int devctl_switchto_thread(const int fd, unsigned tid);
int devctl_get_thread_status(const int fd, unsigned tid, procfs_status *const status);



/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    int res;
    int cpid = -1;

    if (argn == 1) {
        /* debugger */
        res = debugger(&cpid, argv);

        if (res == 0) {
            waitpid(cpid, NULL, 0);
        } else {
            if (cpid ==-1) {
                /* kill the child */
                kill(cpid, SIGKILL);
                kill(cpid, SIGCONT);
            }
        }

        if (res == EOK) {
            printf(PASSED"Test PASS"ENDC"\n");
            return EXIT_SUCCESS;
        } else {
            return EXIT_FAILURE;
        }
    
    } else {
        res = debuggee();
    }
}


/********************************/
/* debugger                     */
/********************************/
int debugger(int *pcpid, char* argv[]) {
    char    arg1[] = "debuggee";
    char * __argv[] = {argv[0], arg1, NULL};
    pid_t cpid;
    posix_spawnattr_t   attr;
    
    // spawn the debuggee in state STOPPED
    posix_spawnattr_init(&attr);
    // posix_spawnattr_setxflags(&attr, POSIX_SPAWN_HOLD);
    posix_spawnattr_setaslr(&attr, POSIX_SPAWN_ASLR_DISABLE);

    if (posix_spawn(&cpid, argv[0], NULL, &attr, __argv, NULL) == -1) {
        failed(posix_spawnp, errno);
        return -1;
    }

    *pcpid = cpid;

    // let the debuggee come up, then attach
    usleep_ms(200);
    char buffer[64];
    sprintf(buffer, "/proc/%d/as", cpid);

    int fd = open(buffer, O_RDWR);

    if (fd == -1) {
        failed(open, errno);
        return -1;
    }

    //debug stop the debuggee
    int rc = devctl(fd, DCMD_PROC_STOP, 0, 0, 0 );
    if (rc != EOK) {
        failed(DCMD_PROC_STOP, rc);
        return -1;
    }
    
    procfs_info pinfo;

    if (devctl_get_proc_info(fd, &pinfo) == -1) {
        return -1;
    }

    if (pinfo.num_threads != 3) {
        printf("invalid number of threads\n");
        return -1;
    }

    // procfs_status status;
    struct sigevent event;
    procfs_run run;
    // siginfo_t info;
    sigset_t set;

    sigemptyset( &set );
    sigaddset( &set, SIGUSR2 );
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (register_event(fd, &event) == -1) {
        failed(register_event, errno);
        return -1;
    }

    event.sigev_value.sival_int = cpid;
    init_run(&run);

    if (devctl_switchto_thread(fd, 2)!= EOK) {
        return -1;
    }

    /* stop at breakpoint() in thread 2*/
    if (set_break_and_wait(cpid, fd, (uintptr_t)breakpoint, &run, &set) != EOK) {
        test_failed;
        return -1;

    }

    procfs_status t3;

    if (devctl_get_thread_status(fd, 3, &t3) != EOK) {
        return -1;
    }

    uintptr_t t3ip = t3.ip;

    for (unsigned u = 0; u < 10; u++) {
        if (set_break_and_wait(cpid, fd, (uintptr_t)breakpoint, &run, &set) != EOK) {
            test_failed;
            return -1;

        }

        if (devctl_get_thread_status(fd, 3, &t3) != EOK) {
            return -1;
        }

        if (t3.ip == t3ip) {
            test_failed;
        }
    }

    //continue process
    if (devctl_run_process(fd, &run) == -1) {
        return -1;
    }

    close(fd);

    return 0;
}


/********************************/
/* set_break_and_wait           */
/********************************/
int set_break_and_wait(int cpid, int fd, uintptr_t brk_addr, procfs_run *prun, sigset_t *pset) {
    siginfo_t       info;
    procfs_status   status = {};

    // printf("%s 0x%lx\n", __func__, brk_addr);
    // fflush(stdout);

    //set breakpoint
    if (devcl_set_breakpoint(fd, brk_addr) == -1) {
        return -1;
    }

    //continue process
    if (devctl_run_process(fd, prun) == -1) {
        return -1;
    }

    //wait for debug event
    sigwaitinfo(pset, &info);

    if (devctl_get_proc_status(fd, &status) == -1) {
        return -1;
    }

    if (/*info.si_value.sival_int != cpid ||*/ status.ip != brk_addr) {
        printf("break at 0x%lx, expected 0x%lx\n", status.ip, brk_addr);
        return -1;
    }

    return EOK;
}

/********************************/
/* register_event               */
/********************************/
int register_event(int fd, struct sigevent *pevent) {
    SIGEV_SIGNAL_THREAD_INIT(pevent, SIGUSR2, 0, 0);

    if (MsgRegisterEvent(pevent, -1) != EOK) {
        failed(MsgRegisterEvent, errno);
        return -1;
    }
    
    if (devctl( fd, DCMD_PROC_EVENT, pevent, sizeof(*pevent), 0) != EOK) {
        failed(DCMD_PROC_EVENT, errno);
        return -1;
    }

    return 0;
}


/********************************/
/* init_run                     */
/********************************/
void init_run(procfs_run *prun) {
    prun->flags = _DEBUG_RUN_TRACE | _DEBUG_RUN_FAULT;
    prun->flags |= _DEBUG_RUN_ARM|_DEBUG_RUN_CLRFLT|_DEBUG_RUN_CLRSIG;

    sigemptyset((sigset_t *)&prun->fault);
    sigaddset((sigset_t *)&prun->fault, FLTILL);
    sigaddset((sigset_t *)&prun->fault, FLTPRIV);
    sigaddset((sigset_t *)&prun->fault, FLTBPT);
    sigaddset((sigset_t *)&prun->fault, FLTTRACE);
    sigaddset((sigset_t *)&prun->fault, FLTBOUNDS);
    sigaddset((sigset_t *)&prun->fault, FLTIOVF);
    sigaddset((sigset_t *)&prun->fault, FLTIZDIV);
    sigaddset((sigset_t *)&prun->fault, FLTFPE);
    sigaddset((sigset_t *)&prun->fault, FLTACCESS);
    sigaddset((sigset_t *)&prun->fault, FLTSTACK);
    sigaddset((sigset_t *)&prun->fault, FLTPAGE);

    sigemptyset((sigset_t *)&prun->trace);
    for (int signo = 0; signo < NSIG; signo++) {
        sigaddset (&prun->trace, signo);
    }

}

void *t1(void *arg);
void *t2(void *arg);
void sigint_handler(int signal);

bool stopit = false;

static volatile uint32_t x;
/********************************/
/* breakpoint                   */
/********************************/
void breakpoint(uint32_t u) {
    x = u;
}

/********************************/
/* debuggee                     */
/********************************/
int debuggee() {
    signal(SIGINT, sigint_handler);
    
    pthread_t tids[2];
    int ret = pthread_create(&tids[0], NULL, t1, NULL);
    if (ret != EOK) {
        failed(pthread_create, errno);
        return EXIT_FAILURE;
    }

    ret = pthread_create(&tids[1], NULL, t2, NULL);
    if (ret != EOK) {
        failed(pthread_create, errno);
        return EXIT_FAILURE;
    }

    void *t1_res;
    pthread_join(tids[0], &t1_res);
    pthread_join(tids[1], NULL);

    switch (*((long*)t1))
    {
    case -1:
        printf("Test ABORT\n");
        break;
    case 0:
        printf("Test SUCCESS\n");
        break;
    default:
        printf("Test FAILED\n");
        break;
    }

    return EOK;
}


/********************************/
/* t1                           */
/********************************/
void *t1(void *arg) {
    while (!stopit)
    {
        printf("%s running\n", __func__);
        breakpoint(1);
        usleep(1000000);
    }
    
    return NULL;
}

/********************************/
/* t2                           */
/********************************/
void *t2(void *arg) {

    while (!stopit)
    {
        printf("%s running\n", __func__);
        breakpoint(2);
        usleep(500000);
    }    

    return NULL;
}

/********************************/
/* sigint_handler               */
/********************************/
void sigint_handler(int signal) {
    stopit = true;
}


/********************************/
/* devcl_set_breakpoint         */
/********************************/
int devcl_set_breakpoint(const int fd, const uintptr_t addr)
{
    procfs_break brk;
    brk.type = _DEBUG_BREAK_EXEC;
    brk.addr = (uintptr_t)addr;
    brk.size = 0;
    int rc = devctl( fd, DCMD_PROC_BREAK, &brk, sizeof brk, 0 );
    if(rc) {
        failed(DCMD_PROC_BREAK, rc);
        return -1;
    }

    return EOK;
}


/********************************/
/* devcl_rm_breakpoint         */
/********************************/
int devcl_rm_breakpoint(const int fd, const uintptr_t addr)
{
    procfs_break brk;
    brk.type = _DEBUG_BREAK_EXEC;
    brk.addr = (uintptr_t)addr;
    brk.size = -1;
    int rc = devctl( fd, DCMD_PROC_BREAK, &brk, sizeof brk, 0 );
    if(rc) {
        failed(DCMD_PROC_BREAK, rc);
        return -1;
    }

    return EOK;
}


/********************************/
/* devctl_run_process           */
/********************************/
int devctl_run_process(const int fd, procfs_run * run)
{
    int rc = devctl( fd, DCMD_PROC_RUN, run, sizeof(procfs_run), 0 );
    if (rc) {
        failed(DCMD_PROC_RUN, rc);
        return -1;
    }

    return EOK;
}


/********************************/
/* devctl_get_proc_status       */
/********************************/
int devctl_get_proc_status(const int fd, procfs_status *const status)
{
    int rc = devctl( fd, DCMD_PROC_STATUS, status, sizeof(procfs_status), 0 );
    if (rc) {
        failed(DCMD_PROC_STATUS, rc);
        return -1;
    }

    return EOK;
}



/********************************/
/* devctl_get_thread_status      */
/********************************/
int devctl_get_thread_status(const int fd, unsigned tid, procfs_status *const status)
{
    status->tid = tid;

    int rc = devctl( fd, DCMD_PROC_TIDSTATUS, status, sizeof(procfs_status), 0 );
    if (rc) {
        failed(DCMD_PROC_TIDSTATUS, rc);
        return -1;
    }

    return EOK;
}



/********************************/
/* devctl_get_proc_info         */
/********************************/
int devctl_get_proc_info(const int fd, procfs_info *const info)
{
    int rc = devctl( fd, DCMD_PROC_INFO, info, sizeof(procfs_info), 0 );
    if (rc) {
        failed(DCMD_PROC_INFO, rc);
        return -1;
    }

    return EOK;
}

// Make sure to switch to the target
// process's main thread.
int devctl_switchto_thread(const int fd, unsigned tid)
{
    int rc = devctl( fd, DCMD_PROC_CURTHREAD, &tid, sizeof tid, 0 );
    if (rc) {
        failed(DCMD_PROC_CURTHREAD, errno);
        return -1;
    }
    return EOK;
}

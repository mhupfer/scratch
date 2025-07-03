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
#include <unistd.h>
#include <sys/neutrino.h>
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
#include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
#include <sys/procfs.h>
// #include <sys/trace.h>
// #include <spawn.h>

// build: qcc test_debug_thread_info.c -o test_debug_thread_info -Wall -g

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define mssleep(t) usleep((t * 1000UL))

#if 1
#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
    }
#endif

int test_proc_status_no_cur_thread();
int test_debug_termer();

void *tfunc(void *_arg) {
    mssleep(1);
    return NULL;
}

/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    int ret = 0;
    if (test_proc_status_no_cur_thread()) {
        test_failed;
        ret = -1;
    }

    if (test_debug_termer()) {
        test_failed;
        ret = -1;
    }

    if (ret == 0)
        printf(PASSED "Test PASS" ENDC "\n");
}

/********************************/
/* test_proc_status_no_cur_thread                     */
/********************************/
int
test_proc_status_no_cur_thread() {
    //check if we get a proc status if the current
    // thread is not yet set
    int ret = 0;

    int pid = fork();
    if (pid == -1)
        return -1;
    else if (pid == 0) {
        /* Don't do anything in child.  */
        pause();
    } else {
        char as_path[256];
        sprintf(as_path, "/proc/%d/as", pid);
        int fd = open(as_path, O_RDWR);
        if (fd == -1) {
            printf("error opening fd for %s: %s\n", as_path, strerror(errno));
            return -1;
        }

        ret = devctl(fd, DCMD_PROC_STOP, NULL, 0, 0);
        if (ret != 0) {
            failed(DCMD_PROC_STOP, ret);
            return -3;
        }

        procfs_status status;
        ret = devctl(fd, DCMD_PROC_STATUS, &status, sizeof(procfs_status), 0);
        if (ret != 0) {
            printf("error getting status\n");
            return -1;
        }

        if (status.tid == 0) {
            printf("FAIL: tid is 0!\n");
            ret = -1;
        }
        else
            printf("PASS: tid is %d\n", status.tid);
    }

    if (kill(pid, SIGTERM) == -1) {
        failed(kill, errno);
    }

    return ret;
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
		printf("devctl(DCMD_PROC_RUN) failed: %s\n", strerror(res));
    }
    return res;
}


/********************************/
/* f_another_thread             */
/********************************/
void * f_another_thread(void *arg) {
    usleep(100*1000);
    return NULL;
}

/********************************/
/* test_debug_termer            */
/********************************/
int test_debug_termer() {
    char path[64];
    int testres = EOK;

    /* create a child. The child sleeps to let the debugger attach,
        then it creates another thread and exits

        Debugger: attach, stop 
        Register an event and let it continue
        wait for thread event -> cet current thread to 2
        wait for the EXIT event. The child is now terminating
        Try procfs_status with tid 2
        Try procfs status with tid 1
        try to set a bp
    */


    int cpid = fork();

    switch (cpid)
    {
    case -1:
        perror("fork");
        return 1;
        break;
    case 0: 
        /* child */
        pthread_t another_thread;

        int res = pthread_create(&another_thread, NULL, f_another_thread, NULL);
        if (res == EOK) {
            pthread_join(another_thread, NULL);
        } else {
            failed(pthread_create, res);
        }
        exit(0);
        break;
    default:
        break;
    }

    /* debug the child */
    sprintf(path, "/proc/%d/as", cpid);

    int fd = open(path, O_RDWR);

    if (fd != -1) {
        debug_thread_t	status;
        int             res;	

        /* stop it and switch to thread 2*/
        for (;;) {
            res = devctl(fd, DCMD_PROC_STOP, &status, sizeof status, 0);

    	    if (res != EOK) {
                failed(devctl(PROC_STOP), res);

                break;
    	    }

            int tid = 2;

            res = devctl(fd, DCMD_PROC_CURTHREAD, &tid, sizeof tid, 0);

    	    if (res == EOK) {
                printf("Switched to thread %d\n", tid);
                break;
            } else {
                failed(devctl(DCMD_PROC_CURTHREAD), res);
                /* try again */
                continue_proc(fd);
    	    }
        }

        if (res == 0) {
            printf("Process stopped\n");
            // Define a sigevent for process termination notification.
            struct sigevent     event;
            event.sigev_notify = SIGEV_SIGNAL_THREAD;
            event.sigev_signo = SIGUSR2;
            event.sigev_code = 0;
            event.sigev_value.sival_ptr = 0;
            event.sigev_priority = -1;

            if (MsgRegisterEvent(&event, fd) == 0) {
                res = devctl( fd, DCMD_PROC_EVENT, &event, sizeof(event), NULL );

                if (res != 0) {
	        	    printf("devctl(DCMD_PROC_EVENT) failed: %s\n", strerror(res));
                }
            } else {
                printf("MsgRegisterEvent() failed: %s\n", strerror(res));
            }

	        if (continue_proc(fd) == 0) {
                printf("Process continues\n");
            }
        }

        sigset_t set;
        int      sig;
        sigemptyset(&set);
        sigaddset(&set, SIGUSR2);

        sigwait(&set, &sig);

        /* get the process status */
        procfs_status		pstatus;

        /* current thread is 2, DCMD_PROC_STATUS  should fall back to thread 1 */
        res = devctl(fd, DCMD_PROC_STATUS, &pstatus, sizeof pstatus, 0);

        if (res == EOK) {
            if (pstatus.why != _DEBUG_WHY_TERMINATED) {
                printf("Child not terminating\n");
                test_failed;
                return -1;
            }

            if (pstatus.tid != 1) {
                // that's not the termer thread
                test_failed;
                testres = -1;
            }

        } else {
            if (res == ESRCH)
                printf("Test FAILED\n");
            else
                printf("devctl(DCMD_PROC_STATUS) failed: %s\n", strerror(res));
        }

        // extract info about the termer as a non-debugger, 
        //needed by servermonitor
        int monpid = fork();

        switch (monpid)
        {
        case -1:
            failed(fork, errno);
            break;
        case 0:
            pstatus.tid = 1;
            res = devctl(fd, DCMD_PROC_TIDSTATUS, &pstatus, sizeof pstatus, 0);

            if (res == EOK) {
                if (pstatus.tid != 1) {
                    exit(1);
                }

            } else {
                if (res == ESRCH)
                    printf("Test FAILED\n");
                else
                    printf("devctl(DCMD_PROC_STATUS) failed: %s\n", strerror(res));

                exit(1);
            }
            exit(0);
        default:
            waitpid(monpid, &res, 0);

            if (res != 0) {
                test_failed;
                testres = -1;
            }
            break;
        }

        //detach debugger
        close(fd);

        // usleep(1000000000);
        // printf("Closing debugger\n");
        //close(fd);
    } else {
        perror("open failed: ");
    }

    waitpid(cpid, 0, 0);
    return testres;
}
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/neutrino.h>
#include <fcntl.h>
#include <sys/debug.h>
#include <sys/procfs.h>
#include <poll.h>
#include <sys/procfs.h>
#include <sys/trace.h>
#include <pthread.h>

//qcc test_debug_terminating_proc.c -o test_debug_terminating_proc -Wall -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


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
/* main                         */
/********************************/
int main(int argc, char **argv) {
    char path[64];

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
        return 0;
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
        trace_logi(42, 1, 0);
        printf("Signal received: %d\n", sig);

        /* get the process status */
        procfs_status		pstatus;

        /* without fix DCMD_PROC_STATUS fails since thread 2 doesn't exist anymore*/
        res = devctl(fd, DCMD_PROC_STATUS, &pstatus, sizeof pstatus, 0);

        if (res == EOK) {
            printf("state=%hhu why=0x%x what= 0x%x\n", pstatus.state, pstatus.why, pstatus.what);
            printf("Test PASSED\n");
        } else {
            if (res == ESRCH)
                printf("Test FAILED\n");
            else
                printf("devctl(DCMD_PROC_STATUS) failed: %s\n", strerror(res));
        }

        // continue_proc(fd);
        //detach debugger
        close(fd);
    } else {
        perror("open failed: ");
    }

    waitpid(cpid, 0, 0);
    return 0;
}
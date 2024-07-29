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

unsigned global_var = 0;


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
        break;
    default:
        usleep(100*1000);
        printf("Write to variable\n");
        trace_logi(42, 0, 0);
        global_var++;
        waitpid(cpid, 0, 0);
        return 0;
        break;
    }

    int debuggee = getppid();
    printf("debuggee pid %d debugger %d\n", debuggee, getpid());
    fflush(stdout);

    sprintf(path, "/proc/%d/as", debuggee);

    int fd = open(path, O_RDWR);

    if (fd != -1) {
        debug_thread_t	status;
    	debug_break_t	brk;
        int             res;	

        res = devctl(fd, DCMD_PROC_STOP, &status, sizeof status, 0);

    	if (res != 0) {
    		printf("devctl(PROC_STOP) failed: %s", strerror(errno));
    	}

        if (res == 0) {
            printf("Process stopped\n");
	        brk.type = _DEBUG_BREAK_WR;
	        brk.size = sizeof(global_var);
	        brk.addr = (uintptr64_t)&global_var;

            res = devctl(fd, DCMD_PROC_BREAK, &brk, sizeof brk, 0);
	        if (res != 0) {
	        	printf("devctl(DCMD_PROC_BREAK) failed: %s\n", strerror(res));
	        } else {
                printf("Breakpoint set\n");
            }


            // Define a sigevent for process stopped notification.
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

        // if (kill(debuggee, SIGSEGV) == -1) {
        //     printf("kill() failed: %s\n", strerror(res));
        // }

        continue_proc(debuggee);

        // usleep(1000000000);
        // printf("Closing debugger\n");
        //close(fd);
    } else {
        perror("open failed: ");
    }
}
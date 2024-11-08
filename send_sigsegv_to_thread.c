#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>

//build: 

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))



/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    struct sigevent event;

    // SIGEV_SIGNAL_THREAD_INIT(&event, SIGSEGV, 1, 1);
    SIGEV_SIGNAL_INIT(&event, SIGSEGV);

    int tid = TimerCreate(CLOCK_MONOTONIC, &event);

    if (tid != -1) {
        const struct _itimer itime = {.nsec = 1000000000, .interval_nsec = 0};

        if (TimerSettime(tid, 0, &itime, NULL) != -1) {
        	sigset_t set;
	        siginfo_t info;
            
    	    sigemptyset(&set);
	        sigaddset(&set, SIGSELECT);

            // usleep(10000000);
            SignalWaitinfoMask(&set, &info, NULL);
        } else {
            failed(TimerSettime, errno);
        }

    } else {
        failed(TimerCreate, errno);
    }
    
}

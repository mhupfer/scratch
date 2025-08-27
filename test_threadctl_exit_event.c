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
#include <pthread.h>
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

//build: qcc test_threadctl_exit_event.c -o test_threadctl_exit_event -Wall -g

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

/********************************/
/* threadfunc                   */
/********************************/
void *threadfunc(void *a) {
    usleep(500);
    return NULL;
}

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    /**
     * Create a thread with a lower prio than the main thread
     * Start it in suspended mode and register an exit event with it
     * Let the thread continue.
     * The main thread joins the other thread and sends an event after 
     * the join.
     * If the event from the main thread arrives before the event from
     * the created thread, we have an issue with the event order.
     */
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    if (pthread_attr_setsuspendstate_np( & attr, PTHREAD_CREATE_SUSPENDED) != EOK) {
        failed("pthread_attr_setsuspendstate_np", errno);
        return EXIT_FAILURE;
    }

    if (pthread_setschedprio(pthread_self(), 9) == -1) {
        failed("pthread_setschedprio", errno);
        return EXIT_FAILURE;
    }

    int chid = ChannelCreate(0);

    if (chid == -1) {
        failed(ChannelCreate, errno);
        return EXIT_FAILURE;
    }

    int coid = ConnectAttach(0, 0, chid, 0, 0);

    if (coid == -1) {
        failed(ConnectAttach, errno);
        return -EXIT_FAILURE;
    }

    struct sigevent thread_ev, main_ev;

    SIGEV_PULSE_INIT(&thread_ev, coid, -1, 17, 0);
    SIGEV_PULSE_INIT(&main_ev, coid, -1, 18, 0);

    for(unsigned loops = 0; loops < 100000;loops++) {
        pthread_t   t;

        if (pthread_create(&t, &attr, threadfunc, NULL) == -1) {
            failed(pthread_create, errno);
            return -EXIT_FAILURE;
        }

        if (ThreadCtlExt(0, 2, _NTO_TCTL_ADD_EXIT_EVENT, &thread_ev) == -1) {
            failed(ThreadCtlExt(0, 2, _NTO_TCTL_ADD_EXIT_EVENT, NULL), errno);
            return -EXIT_FAILURE;
        }

        if (ThreadCtlExt(0, 2, _NTO_TCTL_ONE_THREAD_CONT, NULL) == -1) {
            failed(ThreadCtlExt(0, 2, _NTO_TCTL_ONE_THREAD_CONT, NULL), errno);
            return -EXIT_FAILURE;
        }

        pthread_join(t, NULL);

        if (MsgSendPulse(coid, -1, main_ev.sigev_code, 0) == -1) {
            failed(MsgSendPulse, errno);
            return -EXIT_FAILURE;
        }

        struct _pulse p;

        if (MsgReceivePulse(chid, &p, sizeof(p), NULL) == -1) {
            failed(MsgReceivePulse, errno);
            return -EXIT_FAILURE;
        }

        if (p.code == main_ev.sigev_code) {
            printf(FAILED"Test FAILED"ENDC" at %s, %d, %u loops\n", __func__, __LINE__, loops);
            return EXIT_FAILURE;
        } else {
            if (p.code == thread_ev.sigev_code) {
                struct _pulse p2;

                if (MsgReceivePulse(chid, &p2, sizeof(p), NULL) == -1) {
                    failed(MsgReceivePulse, errno);
                    return EXIT_FAILURE;
                }

                if (p2.code != main_ev.sigev_code) {
                    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);
                    return EXIT_FAILURE;
                }

            } else {
                printf(FAILED"Test Abort"ENDC" at %s, %d\n", __func__, __LINE__);
            }
        }
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

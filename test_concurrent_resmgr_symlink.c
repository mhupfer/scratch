/**
 * @brief stupid resmgr
 * @author Michael Hupfer
 * @email mhupfer@blackberry.com
 * creation date 2025/01/30
 */

// #include "procfs2mgr.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>
#include <sys/trace.h>
#include <sys/pathmgr.h>

// qcc test_concurrent_resmgr_symlink.c -o test_concurrent_resmgr_symlink -Wall -g -L ~/mainline/stage/nto/x86_64/lib/ -I ~/mainline/stage/nto/usr/include/



#define failed(f, e) printf("%s: %s() failed: %s\n", __func__, #f, strerror(e))

/********************/
/* procfs2mgr_t     */
/********************/
typedef struct {
    struct {
        unsigned attach_flags;
        dispatch_context_t *ctp;
    } resmgr;
} stupid_resmg_r;

stupid_resmg_r glob = {0};


#define TARGET "/system/bin/cp"
#define LINK "/data/home/root/link_to_something"
#define MAX_LOOPCNT     1000000

void *t1(void *arg);
void *t2(void *arg);
void sigint_handler(int signal);

unsigned max_loopcnt = MAX_LOOPCNT;
volatile int t1_result = -1;

resmgr_connect_funcs_t connect_funcs;
resmgr_io_funcs_t io_funcs;

/********************************/
/* main                         */
/********************************/
int
main(int argc, char *argv[]) {
    int ret = 0;

    if (argv[1]) {
        max_loopcnt = atoi(argv[1]);
    }

    unlink(LINK);

    int fd = open(TARGET, O_RDONLY);
    if (fd == -1) {
        printf("Cannot execute test %s missing\n", TARGET);
        return EXIT_FAILURE;
    }

    close(fd);

    signal(SIGINT, sigint_handler);
    
    pthread_t tids[2];
    ret = pthread_create(&tids[0], NULL, t1, NULL);
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

    switch (*((int64_t*)t1))
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

    // trace_logf(1, "%s:", "resmgr_attach");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}


#define INVALID_RESULT  1777777

volatile unsigned stop_it = 0;
volatile unsigned t1_ready = 0;
volatile unsigned t2_ready = 0;
volatile unsigned t2_loopcnt = 0;
volatile signed t2_result = INVALID_RESULT;
struct timespec t2_result_time;
volatile int t2_errno = 0;
dispatch_t *dpp2;

#define US(ts) ((ts).tv_sec * 100000 + (ts).tv_nsec / 1000)

#define LOOPCNT_STEP    1
#define THRESHOLD_US    250

/********************************/
/* t1                           */
/********************************/
void *t1(void *arg) {
    unsigned loopcount_init_val = LOOPCNT_STEP;
    struct timespec t;

    t1_ready = 1;
    __sync_synchronize();
    while(t2_ready == 0);

    while (stop_it == 0)
    {
        // printf("Loopcnt: %u\n", loopcount_init_val);

        t2_result = INVALID_RESULT;
        t2_loopcnt = loopcount_init_val;
        __sync_synchronize();

        t1_result = pathmgr_symlink(TARGET, LINK);
        clock_gettime(CLOCK_MONOTONIC, &t);

        while (t2_result == INVALID_RESULT);

        if (t1_result != -1 && t2_result != -1) {
            printf("Both t1 and t2 pathmgr_symlink succeeded (%d, %d)\n", t1_result, t2_result);
            stop_it = 1;
            return (void*)1;
        }

        if (t1_result == -1 && t2_result == -1) {
            printf("Both t1 and t2 pathmgr_symlink failed (%s, %s)\n", strerror(errno), strerror(t2_errno));
            stop_it = 1;
            return (void*)NULL;
        }

        if (unlink(LINK) == -1) {
            failed(unlink, errno);
            stop_it = 1;
            return (void*)-1;
        }
    
        uint64_t t1_us = US(t);
        uint64_t t2_us = US(t2_result_time);

        if (t2_us > t1_us) {
            if (t2_us - t1_us > THRESHOLD_US) {
                if (loopcount_init_val > 1) {
                    loopcount_init_val --;
                    printf("Loopcnt adjusted to: %u\n", loopcount_init_val);
                }
            }
        } else {
            if (t1_us - t2_us > THRESHOLD_US) {
                loopcount_init_val++;
                printf("Loopcnt adjusted to: %u\n", loopcount_init_val);
            }
        }

        if (loopcount_init_val > max_loopcnt) {
            stop_it = 1;
            break;
        }
    }

    return NULL;
}

/********************************/
/* t2                           */
/********************************/
void *t2(void *arg) {

    t2_ready = 1;
    __sync_synchronize();
    while(t1_ready == 0);

    while (stop_it == 0)
    {
        //wait for start signal
        while(t2_loopcnt == 0 && stop_it == 0);

        if (stop_it) {
            break;
        }

        //loop as requested
        while((--t2_loopcnt) > 0);

        // printf("t2 rsmgr_attach\n");
        //try the attach
        t2_result = pathmgr_symlink(TARGET, LINK);
        t2_errno = errno;
        clock_gettime(CLOCK_MONOTONIC, &t2_result_time);
        __sync_synchronize();
    }
    

    return NULL;
}

/********************************/
/* sigint_handler               */
/********************************/
void sigint_handler(int signal) {
    stop_it = 1;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/trace.h>
#include <time.h>
#include <process.h>
#include <stdint.h>

// static void _spin() {
//     struct timespec ts;

//     ts.tv_sec = 0;
//     ts.tv_nsec = rand();
//     nanospin(&ts);
// }

//ntoaarch64-gcc rwlock_reader_priority.c -o rwlock_reader_priority_aarch64 -Wall
static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
static unsigned         loops = 100000;
static unsigned         value = 0;
static unsigned         affinities[3] = {0};
static unsigned         flags = 0;

#define FLAG_SET_AFF    1

#define IDX_WRITER  0
#define IDX_LOW     1
#define IDX_HIGH    2

static void *
reader_low(void * const arg)
{
    int rc;
    int done = 0;

    pthread_setname_np(pthread_self(), __func__);

    if (flags & FLAG_SET_AFF) {
        rc = ThreadCtlExt_r(0, gettid(), _NTO_TCTL_RUNMASK, (void*)(uintptr_t)(1 << affinities[IDX_LOW]));

        if (rc != EOK) {
            printf("%s set runmaks failed: %s\n", __func__, strerror(rc));
        }
    }

    while (!done) {
        rc = pthread_rwlock_rdlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_rdlock: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }

        if (value == loops) {
            done = 1;
        }

        // _spin();

        rc = pthread_rwlock_unlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_unlock (low reaader): %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

static void *
reader_high(void * const arg)
{
    int rc;
    int done = 0;

    pthread_setname_np(pthread_self(), __func__);

    if (flags & FLAG_SET_AFF) {
        rc = ThreadCtlExt_r(0, gettid(), _NTO_TCTL_RUNMASK, (void*)(uintptr_t)(1 << affinities[IDX_HIGH]));

        if (rc != EOK) {
            printf("%s set runmaks failed: %s\n", __func__, strerror(rc));
        }
    }

    while (!done) {
        rc = pthread_rwlock_rdlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_rdlock: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }

        if (value == loops) {
            done = 1;
        }

        // _spin();
        
        rc = pthread_rwlock_unlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_unlock (high reaader): %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    };
    return NULL;
}

static void *
writer(void * const arg)
{
    int rc;
    int done = 0;

    pthread_setname_np(pthread_self(), __func__);

    if (flags & FLAG_SET_AFF) {
        rc = ThreadCtlExt_r(0, gettid(), _NTO_TCTL_RUNMASK, (void*)(uintptr_t)(1 << affinities[IDX_WRITER]));

        if (rc != EOK) {
            printf("%s set runmaks failed: %s\n", __func__, strerror(rc));
        }
    }

    while (!done) {
        rc = pthread_rwlock_wrlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_wrlock: %s\n", strerror(rc));
            exit(EXIT_FAILURE);
        }

        value++;
        if (value == loops) {
            printf("Writer done\n");
            done = 1;
        }

        rc = pthread_rwlock_unlock(&lock);
        if (rc != 0) {
            fprintf(stderr, "pthread_rwlock_unlock (writer): %s\n",
                    strerror(rc));
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t   tids[3];
    int         rc, i = 0;

    if (argc > 1) {
        //first argument is a comma-separated list of cpus
        char    *p;

        p = strtok(argv[1], ",");

        while(p && i < 3) {
            flags |= FLAG_SET_AFF;
            affinities[i] = atoi(p);
            printf("affinities[%d]=%u\n", i, affinities[i]);
            p = strtok(NULL, ",");
            i++;
        }

    }

    //seed random number generator
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    srand((unsigned int)ts.tv_nsec);

    rc = pthread_create(&tids[0], NULL, reader_low, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    struct sched_param sched = { .sched_priority = 20 };
    pthread_attr_setschedparam(&attr, &sched);

    rc = pthread_create(&tids[1], &attr, reader_high, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    rc = pthread_create(&tids[2], NULL, writer, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    for (unsigned i = 0; i < 3; i++) {
        pthread_join(tids[i], NULL);
    }

    return EXIT_SUCCESS;
}

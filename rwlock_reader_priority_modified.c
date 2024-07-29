#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/trace.h>
#include <stdatomic.h>
#include <process.h>
#include <time.h>
// ntoaarch64-gcc rwlock_reader_priority_modified.c -o rwlock_reader_priority_modified -Wall -O2 -g
// without -O2 compilation will fail
// ntoaarch64-gcc rwlock_reader_priority_modified.c  atomic_fetch_or_explicit.S  -o rwlock_reader_priority_modified -Wall -O2 -g


static sync_t           lock = PTHREAD_RWLOCK_INITIALIZER;
static unsigned         loops = 100000;
static unsigned         value = 0;
static uint64_t         time_sum_ms = 0;
uint64_t                spin_sum = 0;


#define TS_TO_MS(ts) ((ts).tv_sec * 1000 + (ts).tv_nsec / 1000000)

void level() {
    long level = _NTO_IO_LEVEL_1;

    if (ThreadCtl(_NTO_TCTL_IO_LEVEL, (void*)level) == -1) {
        printf("Cannot enter privilege level 1: %s\n", strerror(errno));
    }

}

int _atomic_fetch_or_relaxed(unsigned int *target, unsigned int value, uint64_t *spincount, unsigned cpu);


// int _atomic_fetch_or_explicit(unsigned int *target, unsigned int value, int memory_order) {

//     int result, tmp;
    
// //    asm volatile("// atomic_add_return\n"
// //     "1:	ldxr	%w0, %2\n"
// //     "	add	%w0, %w0, %w3\n"
// //     "	stlxr	%w1, %w0, %2\n"
// //     "	cbnz	%w1, 1b"
// // 	: "=&r" (result), "=&r" (tmp), "+Q" (target)
// // 	: "Ir" (value)
// // 	: "memory");

//     asm volatile (
//         "1: ldxr %[result], %[target]\n"
//         "   orr x2, %[val], %[result]\n"
//         "   stlxr w3, w2, %[target]\n"
//          "	cbnz  w3, 1b"
//         : [result] "=&r" (result), [target] "+Q" (target)
//         : [val] "r" (value)
//         :"memory", "w2", "w3"
//     );

//     return result;
// }


int _pthread_rwlock_wrlock(sync_t *sync) {
    struct timespec t0, t1;

    InterruptDisable();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    // printf("count %x\n", sync->__u.__count);
#if MY_LOCK
    uint64_t spincount;
    unsigned const count = _atomic_fetch_or_relaxed(&sync->__u.__count, _NTO_SYNC_WAITING, &spincount, gettid());
    spin_sum += spincount;
    // if (spincount > 1) {
    //     printf("count2 %x %x spins %lx\n", count, sync->__u.__count, spincount);
    // }
#else
    asm volatile("nop\n");
    unsigned const count = atomic_fetch_or_explicit(&sync->__u.__count, _NTO_SYNC_WAITING, memory_order_relaxed);
    asm volatile("nop\n");
#endif
    clock_gettime(CLOCK_MONOTONIC, &t1);
    InterruptEnable();
    time_sum_ms += TS_TO_MS(t1) - TS_TO_MS(t0);
     if ((count & ~_NTO_SYNC_SHARED) == 0U) {
        sync->__owner = gettid();
        return 0;
     } else {
        return EAGAIN;
     }
}


static int
reader_can_lock(sync_t * const      sync)
{
    // If the _NTO_SYNC_WAITING is clear then the lock is neither owned for
    // writing nor waited on by a writer.
    // Of course, the user-mode value may have been corrupted but that's not our
    // problem.
    if ((sync->__u.__count & _NTO_SYNC_WAITING) == 0U) {
        return 1;
    }

    // Check if the lock is held by a writer.
    if (sync->__owner != 0U) {
        return 0;
    }

    return 1;
}

static int
add_reader(sync_t * const sync)
{
    // Get the current count and check for a writer.
    unsigned const count = atomic_load(&sync->__u.__count);

    for (;;) {
        if ((count & _NTO_SYNC_COUNTMASK) == _NTO_SYNC_COUNTMASK) {
            return EAGAIN;
        }

        // Try incrementing the number of readers.
        // On failure count is updated to the latest value.
        if (atomic_compare_exchange_strong_explicit(&sync->__u.__count,
                                                    &count,
                                                    count + 1U,
                                                    memory_order_acquire,
                                                    memory_order_relaxed)) {
            return EOK;
        }

        // Failed to update, try again.
    }
}


int _pthread_rwlock_unlock(sync_t *rwlock, int is_writer) {

    if (is_writer) {
    // Lock is owned exclusively, only the owner can unlock.
        rwlock->__owner = 0U;
        atomic_fetch_and_explicit(&rwlock->__u.__count, ~_NTO_SYNC_WAITING, memory_order_release);
        return 0;
    }


    unsigned const count = atomic_load(&rwlock->__u.__count);

    for (;;) {
        // Check for underflow.
        if ((count & _NTO_SYNC_COUNTMASK) == 0U) {
            return EINVAL;
        }

        // Try decrementing the number of readers.
        // On failure count is updated to the latest value.
        // Since readers do not update state memory unlocking does not have
        // release semantics.
        if (atomic_compare_exchange_strong_explicit(&rwlock->__u.__count,
                                                    &count,
                                                    count - 1U,
                                                    memory_order_relaxed,
                                                    memory_order_relaxed)) {
            break;
        }

        // Failed to update, try again.
    }

    return 0;
}

static void *
reader_low(void * const arg)
{
    int rc;
    int done = 0;

    level();

    while (!done) {

        if (reader_can_lock(&lock)) {
            while(add_reader(&lock));

            if (value == loops) {
                done = 1;
            }

            rc = _pthread_rwlock_unlock(&lock, 0);
            if (rc != 0) {
                fprintf(stderr, "pthread_rwlock_unlock (low reader): %s\n",
                        strerror(rc));
                exit(EXIT_FAILURE);
            }
        } else {
            if (value == loops) {
                printf("rl: lock.__u.__count=%x\n", lock.__u.__count);
                done = 1;
            }
        }
    }
    return NULL;
}

static void *
reader_high(void * const arg)
{
    int rc;
    int done = 0;

    level();

    while (!done) {
        if (reader_can_lock(&lock)) {
            add_reader(&lock);

            if (value == loops) {
                done = 1;
            }

            rc = _pthread_rwlock_unlock(&lock, 0);
            if (rc != 0) {
                fprintf(stderr, "pthread_rwlock_unlock (high reader): %s\n",
                        strerror(rc));
                exit(EXIT_FAILURE);
            }
        } else {
            if (value == loops) {
                printf("rh: lock.__u.__count=%x\n", lock.__u.__count);
                done = 1;
            }
        }
    }
    return NULL;
}

static void *
writer(void * const arg)
{
    int done = 0;

    level();

    while (!done) {
        while (!_pthread_rwlock_wrlock(&lock));

        value++;
        // if ((value % 1000) == 0)
        //     printf("%u\n", value);

        if (value == loops) {
            printf("Writer done\n");
            done = 1;
        }

        _pthread_rwlock_unlock(&lock, 1);
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    pthread_t   tids[3];
    int         rc;

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

    printf("Time spend for wr locking: %lums\n", time_sum_ms);
#ifdef MY_LOCK
    printf("Average spins for wr locking: %f\n", (float)spin_sum / (float)value);
#endif
    return EXIT_SUCCESS;
}

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <sys/neutrino.h>
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <aio.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <errno.h>

#define PATH "test_aio_suspend"
#define TIMEOUT_SECS 3

void *aio_thread(void *arg);

/********************************/
/* main                         */
/********************************/
int main(int argn, char* argv[]) {
    /* attach a path */
    name_attach_t * na = name_attach( NULL, PATH, 0);
    assert(na != NULL);

    /* open it */
    int fd = name_open(PATH, 0);
    assert(fd != -1);

    /* measure now */
    struct timespec start_of_test;
    clock_gettime(CLOCK_MONOTONIC, &start_of_test);

    /* call aio_supend() with a timeout in a separate thread */
    pthread_t th;
    int res =  pthread_create(&th, NULL, aio_thread,  (void*)(uintptr_t)fd);
    assert(res == 0);

    /* update realtime */
    struct timespec new_time;
    new_time.tv_sec = start_of_test.tv_nsec + TIMEOUT_SECS - 1;
    new_time.tv_nsec = start_of_test.tv_nsec;

    res = clock_settime(CLOCK_REALTIME, &new_time);
    assert(res == 0);

    /* join the tread */
    void *paio_retval;
    pthread_join(th, &paio_retval);

    assert((int)(uintptr_t)paio_retval == EAGAIN);

    /* check expired time: timeout should not be affected by chnahing clock_realtime */
    struct timespec end_of_test;
    clock_gettime(CLOCK_MONOTONIC, &end_of_test);

    if (timespec2nsec(&end_of_test) - timespec2nsec(&start_of_test) >= TIMEOUT_SECS * 1000000000L) {
        printf("pass\n");
    } else {
        printf("failed\n");
    }

    /* clean up */
    name_detach(na, 0);
}

//aio_priv.h
#define _AIO_FLAG_IN_PROGRESS 0x00000002

/********************************/
/* aio_thread                   */
/********************************/
void *aio_thread(void *arg) {
    struct timespec to = {.tv_sec = TIMEOUT_SECS, .tv_nsec = 0};
    struct aiocb acb = {0};
    const struct aiocb * const acb_arr[1] = {&acb};

    acb.aio_fildes = (int)(uintptr_t)arg;
    acb._aio_flag = _AIO_FLAG_IN_PROGRESS; //hack it

    aio_suspend(acb_arr, 1, &to);
    return (void*)(uintptr_t)errno;
}
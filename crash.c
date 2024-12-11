#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

/**
 * Program supposed to create a cortedump by crashing
 * - should always create the same core (deactivate ASLR)
 * - should have multiple threads with a hole in numbering
 * - should have the same registers when crashing
 * - link to en extra so (math)
 * - have defined memory content
 * - should have mapped a file
 * - expose a defined backtrace
 */

/**
 * Build
 * ntoaarch64-gcc crash.c -o crash_aarch64 -Wall -g
 *  ntox86_64-gcc crash.c -o crash_x86_64 -Wall -g
 */

void crash_me();
int map_file();

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

void *mapped_file = MAP_FAILED;
double comp_result = 0;
int waiting_thread();
int exiting_thread(pthread_t *h);

/********************************/
/* main                         */
/********************************/
int
main(int argc, char *argv[]) {
    pthread_t h;

    comp_result = sqrt(42 * 42);
    map_file();

    waiting_thread();
    exiting_thread(&h);
    waiting_thread();
    usleep(2000);
    crash_me();
    pthread_join(h, NULL);
    return EXIT_SUCCESS;
}

/********************************/
/* map_file                     */
/********************************/
int
map_file() {
    int fd = open("/data/var/etc/passwd", O_RDONLY);

    if (fd != -1) {
        mapped_file = mmap(NULL, 0x400, PROT_READ, MAP_SHARED, fd, 0);

        if (mapped_file == MAP_FAILED) {
            failed(mmap, errno);
            return -1;
        }

        close(fd);
    } else {
        failed(open, errno);
        return -1;
    }

    return 0;
}

/********************************/
/* crash_me                     */
/********************************/
void __attribute__((noinline))
crash_me() {
    /* crash with defined
       registers  and backtrace
    */
#if defined(__x86_64__)
    __asm__("movl    $7,%eax"
            "\n"
            "movl    $8,%ebx"
            "\n"
            "movl    $9,%ecx"
            "\n"
            "movl    $10,%edi"
            "\n"
            "movl    %ebx,(%eax)"
            "\n");
#elif defined(__aarch64__)
    __asm__("mov    x0, 7"
            "\n"
            "mov    x2, 8"
            "\n"
            "mov    x3, 9"
            "\n"
            "mov    x4, 10"
            "\n"
            "str    x1, [x0]"
            "\n");
#endif
}

/********************************/
/* waiting_thread_func          */
/********************************/
void *
waiting_thread_func(void *arg) {
    int chid = (int)(long)arg;
    char buffer[128];

    MsgReceive(chid, &buffer, sizeof(buffer), 0);

    return 0;
}

/********************************/
/* waiting_thread               */
/********************************/
int
waiting_thread() {
    pthread_t h;
    int ch = ChannelCreate(0);
    int result;

    if (ch != -1) {
        int result = pthread_create(&h, NULL, waiting_thread_func, (void *)(long)ch);

        if (result == -1) {
            failed(pthread_create, errno);
        }
    } else {
        failed(ChannelCreate, errno);
        result = -1;
    }

    return result;
}


/********************************/
/* exiting_thread_func          */
/********************************/
void *exiting_thread_func(void *arg) {
    usleep(500);
    return NULL;
}

/********************************/
/* exiting_thread               */
/********************************/
int exiting_thread(pthread_t *h) {
    int result = pthread_create(h, NULL, exiting_thread_func, NULL);

    if (result == -1) {
        failed(pthread_create, errno);
    }

    return result;
}
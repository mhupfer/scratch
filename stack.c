#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

void *threadfunc(void *arg);

int main(int argc, char* argv[]) {
    pthread_t       h;
    pthread_attr_t  attr;

    pthread_attr_init(&attr);
    // pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);

    if (pthread_create(&h, &attr, &threadfunc, NULL) == 0) {
        pthread_join(h, NULL);
    } else {
        failed(pthread_Create, errno);
    }

    void * v = mmap(NULL, 0x2000, PROT_READ|PROT_WRITE, MAP_LAZY|MAP_ANON|MAP_PRIVATE, NOFD, 0);

    if (v != MAP_FAILED) {
        *(unsigned*)(v+0x1004) = 0;
    } else {
        failed(mmap, errno);
    }
}


void *threadfunc(void *arg) {
    /* cause a stack overflow */
    char data[8 * 1024];

    data[0] = 0;
    printf("%c\n", data[0]);
    return NULL;
}
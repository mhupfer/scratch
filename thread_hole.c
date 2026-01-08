#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


void *thread_func(void *arg) {
    int id = (int)(long)arg;
    pid_t tid = gettid();

    printf("Thread %d started, TID = %d\n", id, tid);

    if (id == 2) {
        sleep(1);
        printf("Thread %d exiting (TID = %d)\n", id, tid);
        pthread_exit(NULL);
    }

    // Sit and wait forever
    while (1) {
        sleep(10);
    }

    return NULL;
}

int main(void) {
    pthread_t threads[5];

    for (int i = 0; i < 5; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, (void *)(long)i) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    pthread_join(threads[2], NULL);

    // Parent thread waits forever
    while (1) {
        sleep(10);
    }

    return 0;
}

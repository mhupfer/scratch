#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>


int main(int argc, char *argv[]) {
    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, NULL);

    pthread_mutex_lock(&mtx);
    pthread_mutex_unlock(&mtx);
    return EXIT_SUCCESS;
}
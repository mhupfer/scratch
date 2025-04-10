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
#include <stdbool.h>
#include <pthread.h>

//  qcc two_threads.c -o two_threads -Wall -g

#define failed(f, e) printf("%s: %s() failed: %s\n", __func__, #f, strerror(e))


void *t1(void *arg);
void *t2(void *arg);
void sigint_handler(int signal);

bool stopit = false;


/********************************/
/* main                         */
/********************************/
int
main(int argc, char *argv[]) {
    int ret = 0;


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

    switch (*((long*)t1))
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



/********************************/
/* t1                           */
/********************************/
void *t1(void *arg) {
    while (!stopit)
    {
        printf("%s running\n", __func__);
        usleep(500000);
    }
    
    return NULL;
}

/********************************/
/* t2                           */
/********************************/
void *t2(void *arg) {

    while (!stopit)
    {
        printf("%s running\n", __func__);
        usleep(500000);
    }    

    return NULL;
}

/********************************/
/* sigint_handler               */
/********************************/
void sigint_handler(int signal) {
    stopit = true;
}

#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

//build: qcc test_ppoll.c -o test_ppoll -Wall -g -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/x86_64/lib/
int sigusr1_triggered = 0;
int sigchld_triggered = 0;

void sighandler(int signo) {
    if (signo == SIGCHLD) {
        sigchld_triggered++;
    }
    if (signo == SIGUSR1) {
        sigusr1_triggered++;
    }
}

int main(int argc, char *argv[]) {

    /*
        needs a pipe to run
        create a pipe
        create a signal handler for SIGUSR1
        add SIGUSR1 to signalmask
        call ppoll (pipe_fd, original_sigmask )
        unblock ppoll using SIGUSR1
        unblock ppoll using pipe
        check if SIGUSR1 is pending
        check if pipe was triggered
    */

    int             pipe_fd[2];
    sigset_t        blockusr1, original;
    struct sigaction sa;

    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = sighandler;
    
    if (sigaction (SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;

    }

    if (sigaction (SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;

    }

    if (pipe((int*)&pipe_fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    sigemptyset(&blockusr1);
    sigaddset(&blockusr1, SIGUSR1);
    sigaddset(&blockusr1, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &blockusr1, &original) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    int cpid = fork();

    switch (cpid)
    {
    case -1:
        perror("fork()");
        return EXIT_FAILURE;
        // break;
    case 0:
    {
        unsigned char val = 0x42;
        /* child */
        usleep(300000);

        if (kill(getppid(), SIGUSR1) == -1) {
            perror("kill");
            return EXIT_FAILURE;
        }

        usleep(100000);

        if (write(pipe_fd[1], &val, 1) != 1) {
            perror("write");
            return EXIT_FAILURE;
        }

        /* child exit triggers SIGCHLD */
        usleep(100000);
        return EXIT_SUCCESS;
    }
    default:
        break;
    }

    /* parent */
    fd_set readfds;
    struct	timespec timeout = {.tv_sec = 7, .tv_nsec = 0};
    int pipe_triggered = 0;


    while (!(sigusr1_triggered && pipe_triggered && sigchld_triggered))
    {
    	FD_ZERO(&readfds);
        FD_SET(pipe_fd[0], &readfds);

        printf("call pselect ...");
        int rc = pselect(pipe_fd[0] + 1, &readfds, NULL, NULL, &timeout, &original);
        printf("returned %d\n", rc);

        switch (rc)
        {
        case -1:
            if (errno != EINTR) {
                perror("ppoll");
                return EXIT_FAILURE;
            }

            if (sigusr1_triggered) {
                printf("SIGUSR1 received\n");
            }

            if (sigchld_triggered) {
                printf("SIGCHLD received\n");
            }

            break;
        case 0:
            printf("ppoll timeout, sigusr1_triggered=%u, fd_trigerred=%u\n", sigusr1_triggered, pipe_triggered);
            return EXIT_FAILURE;
        default:
            if (FD_ISSET(pipe_fd[0], &readfds)) {
                unsigned char val = 0;
                pipe_triggered++;
                printf("pipe triggered\n");

                if (read(pipe_fd[0], &val, 1) != 1) {
                    perror("read");
                    return EXIT_FAILURE;
                }

            }

            break;
        }
    }
    
    waitpid(cpid, NULL, 0);
    if (sigusr1_triggered && pipe_triggered && sigchld_triggered)
        printf("Pass\n");
    else
        printf("Fail\n");
    
    return EXIT_SUCCESS;
}
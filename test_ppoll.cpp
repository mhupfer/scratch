#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
//#include <time.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

//build: qcc test_ppoll.c -o test_ppoll -Wall -g -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/x86_64/lib/
int sig_triggered = 0;

void sighandler(int signo) {
    if (signo == SIGUSR1) {
        sig_triggered++;
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

    if (pipe((int*)&pipe_fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    sigemptyset(&blockusr1);
    sigaddset(&blockusr1, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &blockusr1, &original) == -1) {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }


    struct pollfd fds = {.fd = pipe_fd[0], .events = POLLIN, .revents = 0};
    struct	timespec timeout = {.tv_sec = 7, .tv_nsec = 0};
    int pipe_triggered = 0;

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

        return EXIT_SUCCESS;
    }
    default:
        break;
    }

    while (!(sig_triggered && pipe_triggered))
    {
        fds.revents = 0;
        
        int rc = ppoll(&fds, 1, &timeout, &original);

        switch (rc)
        {
        case -1:
            if (errno != EINTR) {
                perror("ppoll");
                return EXIT_FAILURE;
            }
            break;
        case 0:
            printf("ppoll timeout, sig_triggered=%u, fd_trigerred=%u\n", sig_triggered, pipe_triggered);
            return EXIT_FAILURE;
        default:
            if (fds.revents & POLLIN) {
                unsigned char val = 0;
                pipe_triggered++;

                if (read(pipe_fd[0], &val, 1) != 1) {
                    perror("read");
                    return EXIT_FAILURE;
                }

            }

            break;
        }
    }
    
    waitpid(cpid, NULL, 0);
    if (sig_triggered ==1 && pipe_triggered)
        printf("Pass\n");
    else
        printf("Fail\n");
    
    return EXIT_SUCCESS;
}
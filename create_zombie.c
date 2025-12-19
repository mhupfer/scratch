#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // Child exits immediately → becomes zombie
        printf("Child exiting to become a zombie...\n");
        exit(0);
    }

    // Parent: do not call wait(), keep running forever
    printf("Parent running. Child PID = %d is now a zombie.\n", pid);

    // Sleep forever so the zombie remains
    while (1) {
        pause();
    }

    return 0;
}
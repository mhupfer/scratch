#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <dlfcn.h>
#include <string.h>

// build: ntoaarch64-gcc ps_in_a_loop.c -o ps_in_a_loop -Wall -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))
int child();

bool run = true;

/********************************/
/* sighandler                   */
/********************************/
void sighandler(int sig) {
    run = false;
}

/********************************/
/* main                         */
/********************************/
int main() {
    FILE *fp;
    char output[1035];

    int cid = fork();

    switch (cid)
    {
    case -1:
        failed(fork, errno);
        break;
    case 0:
        return child();
        break;
    default:
        break;
    }

    signal(SIGINT, sighandler);

    while(run) {
        fp = popen("ps", "r");
        if (fp == NULL) {
            printf("Failed to run command");
            exit(1);
        }

        while (fgets(output, sizeof(output), fp) != NULL) {
            printf("%s", output);
        }

        pclose(fp);
        usleep(100000);
    }

    SignalKill(0, cid, 0, SIGTERM, 0, 0);

    return 0;
}


/********************************/
/* child                        */
/********************************/
int child() {
    
    while (true) {
        int gcid = fork();
        
        switch (gcid)
        {
        case -1:
            failed(fork, errno);
            break;
        case 0: {
            const void *dlh = dlopen("libmtouch-calib.so.1", RTLD_GLOBAL);

            if (dlh == NULL) {
                failed(dlopen, errno);
                exit(EXIT_FAILURE);
            } else {
                usleep(90000);
                exit(EXIT_SUCCESS);
            }
        }
        default:
            int gcid_exit_code;
            waitpid(gcid, &gcid_exit_code, 0);

            if (gcid_exit_code == EXIT_FAILURE) {
                return EXIT_FAILURE;
            }
            break;
        }
    }

    return EXIT_SUCCESS;
}
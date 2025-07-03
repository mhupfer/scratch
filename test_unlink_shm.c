//#/********************************************************************#
//#                                                                     #
//#                       ██████╗ ███╗   ██╗██╗  ██╗                    #
//#                      ██╔═══██╗████╗  ██║╚██╗██╔╝                    #
//#                      ██║   ██║██╔██╗ ██║ ╚███╔╝                     #
//#                      ██║▄▄ ██║██║╚██╗██║ ██╔██╗                     #
//#                      ╚██████╔╝██║ ╚████║██╔╝ ██╗                    #
//#                       ╚══▀▀═╝ ╚═╝  ╚═══╝╚═╝  ╚═╝                    #
//#                                                                     #
//#/********************************************************************#

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>

//build: qcc test_unlink_shm.c -o test_unlink_shm -Wall -g


#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 0
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {

    int fd = shm_open("kacke", O_CREAT | O_RDWR, 0666);

    if (fd == -1) {
        fd = shm_open("kacke", O_RDWR, 0666);
    }

    if (fd == -1) {
        failed(shm_open, errno);
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, 4096) == -1) {
        failed(ftruncate, errno);
        return EXIT_FAILURE;
    }    

    int cpid = fork();

    switch (cpid)
    {
    case -1:
        failed(fork, errno);
        return EXIT_FAILURE;
        break;
    case 0: {
        // child: open shm and hang around forever
        int fd = shm_open("kacke", O_RDWR, 0666);
    
        if (fd == -1) {
            failed(shm_open, errno);
            return EXIT_FAILURE;
        }

        printf("child opened shm\n");
        
        while(true) {
            void *v = mmap(0, 4096, PROT_READ, MAP_SHARED, fd, 0);

            if (v == MAP_FAILED) {
                failed(mmap, errno);
            } else {
                printf("child mapped shm\n");
            }
            munmap(v, 4096);
            usleep_ms(100);
        }
    }
    default:
        break;
    }

    //wait until child opened the shm
    usleep_ms(500);
    printf("parent unlink shm\n");

    //unlink it
    if (unlink("/dev/shmem/kacke") == -1) {
        failed(unlink, errno);
    }
    usleep_ms(500);

    kill(cpid, SIGTERM);

    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

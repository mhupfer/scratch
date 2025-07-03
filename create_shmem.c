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

//build: 

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

int create_persistent_shmem(int counter);
int create_volatile_shmem();

/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    create_volatile_shmem();
    create_persistent_shmem(1);
    return EXIT_SUCCESS;
}


/********************************/
/* create_volatile_shmem        */
/********************************/
int create_volatile_shmem() {
    int shm_fd = shm_open(SHM_ANON, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1) {
        failed(shm_open, errno);
        // if (errno == EEXIST) {
        //     // If the shared memory object already exists, try the next name
        //     return -1;
        // }
        // perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, 4096) == -1) {
        failed(ftruncate, errno);
        close(shm_fd);
        return -1;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);
}

/********************************/
/* create_shmem                 */
/********************************/
int create_persistent_shmem(int counter) {
    char shm_name[32];

    snprintf(shm_name, sizeof(shm_name), "/my_shm_%d", counter);
    
    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1) {
        failed(shm_open, errno);
        // if (errno == EEXIST) {
        //     // If the shared memory object already exists, try the next name
        //     return -1;
        // }
        // perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, 4096) == -1) {
        failed(ftruncate, errno);
        shm_unlink(shm_name);
        close(shm_fd);
        return -1;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}
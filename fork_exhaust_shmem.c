#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//qcc fork_exhaust_shmem.c -o fork_exhaust_shmem -g -Wall  

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define SEGMENT_SIZE 1024 * 1024 // 1 MB

int create_shmem(int counter);

int main(int argc, char* argv[]) {
    int child_res = EXIT_SUCCESS;
    unsigned    init_counter = 0;

    if (argc > 1) {
        init_counter = atoi(argv[1]);
    }

    unsigned counter = init_counter;
    
    while(child_res == EXIT_SUCCESS) {
        counter++;

        int cpid = fork();

        switch (cpid)
        {
        case -1:
            failed(fork, errno);
            return EXIT_FAILURE;
        break;
        case 0:
            return create_shmem(counter) == EOK ?EXIT_SUCCESS : EXIT_FAILURE;
        default:
            waitpid(cpid, &child_res, 0);
            break;
        }
    }

    printf("Created %u shmems\n", counter - init_counter);

    return EXIT_SUCCESS;
}

/********************************/
/* create_shmem                 */
/********************************/
int create_shmem(int counter) {
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

    if (ftruncate(shm_fd, SEGMENT_SIZE) == -1) {
        failed(ftruncate, errno);
        shm_unlink(shm_name);
        close(shm_fd);
        return -1;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}
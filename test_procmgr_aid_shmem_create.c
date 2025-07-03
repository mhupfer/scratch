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
#include <sys/procmgr.h>

//build: qcc test_procmgr_aid_shmem_create.c -o test_procmgr_aid_shmem_create -Wall -g -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))
int create_shmem(int counter, off_t size);
int ftruncate_shmem(int counter);
int fcntl_shmem(int counter);
int shmctl_shmem(int counter);

#define SEGMENT_SIZE 1024 * 1024 // 1 MB
#define SHMEM_PREFIX    "sfsdfdsfdsf"


#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

#define SHMEM_ROOT_SIZED    1
#define SHMEM_NONROOT_SIZED    2
#define SHMEM_TMP    3
#define SHMEM_UNSIZED    4

/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    int     res;
    char    cmd[48];

    snprintf(cmd, sizeof(cmd), "rm /dev/shmem/%s_*", SHMEM_PREFIX);
    system(cmd);

    res = create_shmem(SHMEM_ROOT_SIZED, SEGMENT_SIZE);

    if (res != EOK) {
        test_failed;
    }

    //non sized shm
    res = create_shmem(SHMEM_TMP, 0);

    if (res != EOK) {
        test_failed;
    }

    //size it
    res = shmctl_shmem(SHMEM_TMP);

    if (res != EOK) {
        test_failed;
    }

    //non sized shm
    res = create_shmem(SHMEM_UNSIZED, 0);

    if (res != EOK) {
        test_failed;
    }

    //drop PROCMGR_AID_SHMEM_CREATE
    res = procmgr_ability(0, PROCMGR_ADN_ROOT|PROCMGR_ADN_NONROOT|PROCMGR_AOP_DENY|PROCMGR_AID_EOL|PROCMGR_AID_SHMEM_CREATE);

    if (res != EOK) {
        failed(procmgr_ability, errno);
        exit(8);
    }
    
    res = create_shmem(SHMEM_NONROOT_SIZED, SEGMENT_SIZE);

    if (res != EPERM) {
        test_failed;
    }

    res = ftruncate_shmem(SHMEM_ROOT_SIZED);

    if (res != EPERM) {
        test_failed;
    }

    res = shmctl_shmem(SHMEM_UNSIZED);

    if (res != EPERM) {
        test_failed;
    }

    res = fcntl_shmem(SHMEM_UNSIZED);

    if (res != EPERM) {
        test_failed;
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}


/********************************/
/* create_shmem                 */
/********************************/
int create_shmem(int counter, off_t size) {
    char shm_name[32];
    int ret = EOK;

    snprintf(shm_name, sizeof(shm_name), "/%s_%d", SHMEM_PREFIX, counter);
    
    int shm_fd = shm_open(shm_name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shm_fd == -1) {
        ret = errno;
        failed(shm_open, errno);
        // if (errno == EEXIST) {
        //     // If the shared memory object already exists, try the next name
        //     return -1;
        // }
        // perror("shm_open");
        return ret;
    }

    if (size == 0) {
        return EOK;
    }

    if (ftruncate(shm_fd, size) == -1) {
        ret = errno;
        failed(ftruncate, errno);
        shm_unlink(shm_name);
        close(shm_fd);
        return ret;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}


/********************************/
/* open_stat_shmem              */
/********************************/
int open_stat_shmem(int counter, off_t *psize) {
    char        shm_name[32];
    int         ret = EOK;
    struct stat sbuf;

    snprintf(shm_name, sizeof(shm_name), "/%s_%d", SHMEM_PREFIX, counter);
    
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        ret = errno;
        failed(shm_open, errno);
        return -ret;
    }

    if (fstat(shm_fd, &sbuf) == -1) {
        ret = errno;
        failed(fstat, errno);
        return -ret;
    }

    *psize = sbuf.st_size;
    return shm_fd;
}

/********************************/
/* ftruncate_shmem               */
/********************************/
int ftruncate_shmem(int counter) {
    int     shm_fd;
    off_t   shm_size;

    shm_fd = open_stat_shmem(counter, &shm_size);

    if (shm_fd < 0)
        return -shm_fd;

    if (ftruncate(shm_fd, shm_size + 0x1000) == -1) {
        int ret = errno;
        failed(ftruncate, errno);
        close(shm_fd);
        return ret;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}



/********************************/
/* fcntl_shmem                 */
/********************************/
int fcntl_shmem(int counter) {
    int     shm_fd;
    off_t   shm_size;

    shm_fd = open_stat_shmem(counter, &shm_size);

    if (shm_fd < 0)
        return -shm_fd;

    flock_t flock = {
        .l_len =  shm_size + 0x1000,
        .l_start = 0,
        .l_whence = SEEK_SET
    };

    if (fcntl(shm_fd, F_ALLOCSP, &flock) == -1) {
        int ret = errno;
        failed(fcntl, errno);
        close(shm_fd);
        return ret;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}


/********************************/
/* shmctl_shmem                 */
/********************************/
int shmctl_shmem(int counter) {
    int     shm_fd;
    off_t   shm_size;

    shm_fd = open_stat_shmem(counter, &shm_size);

    if (shm_fd < 0)
        return -shm_fd;

    if (shm_ctl(shm_fd, SHMCTL_ANON, 0, shm_size + 0x1000) == -1) {
        int ret = errno;
        failed(shm_ctl, errno);
        close(shm_fd);
        return ret;
    }

    // printf("Created shared memory segment: %s\n", shm_name);

    close(shm_fd);

    return EOK;
}


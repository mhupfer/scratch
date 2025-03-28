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
#include <sys/neutrino.h>
#include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
#include <sys/procfs.h>
//#include <sys/trace.h>

//build: ntoaarch64-gcc test_mapdebug_and_unmap.c -o test_mapdebug_and_unmap -Wall -g


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

#define test_abort(f, e) failed(f,e);\
exit(1);

bool        run = true;
volatile void        *v_file = MAP_FAILED;
volatile unsigned    g_unmap_cnt = 0;

/********************************/
/* sighandler                   */
/********************************/
void sighandler(int sig) {
    run = false;
}

/********************************/
/* mapdebug_thread              */
/********************************/
void *mapdebug_thread(void *arg) {
    procfs_debuginfo    mapdebug;
    uint64_t            loopcnt = 0;
    
    int fd = open("/proc/self/as", O_RDONLY);

    if (fd == -1) {
        failed(open, errno);
        run = false;
        return NULL;
    }

    while(run) {
        if ((++loopcnt % 10000ULL) == 0) {
            printf("%s %lu loops\n", __func__, loopcnt);
        }

        //map_unmap_thread did the mapping
        while(run && v_file == MAP_FAILED);

        // usleep(random() & 0xff);
        mapdebug.vaddr = (uintptr_t)v_file;
        //signal map_unmap_thread to do the unmap
        //in parallel send the devctl
        g_unmap_cnt++;

        if (devctl(fd, DCMD_PROC_MAPDEBUG, &mapdebug, sizeof (mapdebug), NULL) < 0) {
            failed(devctl, errno);
            run = false;
            return NULL;
        }

        while(run && v_file != MAP_FAILED);
        // tell map_unmap_thread to start another loop
        g_unmap_cnt++;
    }

    return NULL;
}


/********************************/
/* map_unmap_thread            */
/********************************/
void *map_unmap_thread(void *arg) {
    char shmname[] = "/level1/level2/test_mapdebug_and_unmap";
    uint64_t    loopcnt = 0;

    while(run) {
        if ((++loopcnt % 10000ULL) == 0) {
            printf("%s %lu loops\n", __func__, loopcnt);
        }
#ifdef _fdmem
        const int fd = open("/etc/passwd", O_RDONLY);

        if (fd == -1) {
            failed(open, errno);
            return NULL;
        }
#else
        const int fd = shm_open(shmname, O_CREAT | O_RDWR, S_IRWXU);

        if (fd == -1) {
            failed(shm_open, errno);
            return NULL;
        }
        if (ftruncate(fd, 4096) == -1) {
            failed(ftruncate, errno);
            return NULL;
        }    
#endif
        unsigned unmacpnt = g_unmap_cnt;
        const void *v = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        
        if (v == MAP_FAILED) {
            failed(mmap, errno);
            run = false;
            return NULL;
        }

        //let mapdebug_thread send the devctl
        v_file = (void*)v;
        __sync_synchronize();

        //mapdebug_thread has seen v_file != MAP_FAILED
        while(run && unmacpnt == g_unmap_cnt);
        
#ifndef _fdmem
        shm_unlink(shmname);
#endif
        munmap((void*)v_file, 4096);

        unmacpnt = g_unmap_cnt;
        v_file = MAP_FAILED;
        __sync_synchronize();
        //mapdebug_thread has seen v_file == MAP_FAILED
        while(run && unmacpnt == g_unmap_cnt);
    }
    return NULL;
}


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    pthread_t   t1, t2;
    int         rc;

    // srand((unsigned)ClockCycles());
    signal(SIGINT, sighandler);

    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);
    
    // Set the scheduling priority
    param.sched_priority = 10;
    pthread_attr_setschedparam(&attr, &param);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);

    rc = pthread_create(&t1, &attr, mapdebug_thread, NULL);
    
    if (rc == -1) {
        test_abort(pthread_create, errno);
    }
    rc = pthread_create(&t2, &attr, map_unmap_thread, NULL);
    
    if (rc == -1) {
        test_abort(pthread_create, errno);
    }
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

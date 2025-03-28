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
void        *v_file = MAP_FAILED;
unsigned    g_unmap_cnt = 0;

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
    
    int fd = open("/proc/self/as", O_RDONLY);

    if (fd == -1) {
        failed(open, errno);
        run = false;
        return NULL;
    }

    while(run) {
        while(run && v_file == MAP_FAILED);

        // usleep(random() & 0xff);
        mapdebug.vaddr = (uintptr_t)v_file;
        g_unmap_cnt++;
        __sync_synchronize();
        
        if (devctl(fd, DCMD_PROC_MAPDEBUG, &mapdebug, sizeof (mapdebug), NULL) < 0) {
            failed(devctl, errno);
            run = false;
            return NULL;
        }

        while(run && v_file != MAP_FAILED);
    }

    return NULL;
}


/********************************/
/* map_unmap_thread            */
/********************************/
void *map_unmap_thread(void *arg) {
    while(run) {
        const int fd = open("/etc/passwd", O_RDONLY);

        if (fd == -1) {
            failed(open, errno);
            return NULL;
        }

        unsigned unmacpnt = g_unmap_cnt;
        v_file = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
        __sync_synchronize();

        if (v_file == MAP_FAILED) {
            failed(mmap, errno);
            run = false;
            return NULL;
        }

        while(run && unmacpnt == g_unmap_cnt);
        munmap(v_file, 4096);
        v_file = MAP_FAILED;
        __sync_synchronize();
        close(fd);
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

    rc = pthread_create(&t1, NULL, mapdebug_thread, NULL);
    
    if (rc == -1) {
        test_abort(pthread_create, errno);
    }
    rc = pthread_create(&t2, NULL, map_unmap_thread, NULL);
    
    if (rc == -1) {
        test_abort(pthread_create, errno);
    }
    
    signal(SIGINT, sighandler);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

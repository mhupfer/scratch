// #/********************************************************************#
// #                                                                     #
// #                       ██████╗ ███╗   ██╗██╗  ██╗                    #
// #                      ██╔═══██╗████╗  ██║╚██╗██╔╝                    #
// #                      ██║   ██║██╔██╗ ██║ ╚███╔╝                     #
// #                      ██║▄▄ ██║██║╚██╗██║ ██╔██╗                     #
// #                      ╚██████╔╝██║ ╚████║██╔╝ ██╗                    #
// #                       ╚══▀▀═╝ ╚═╝  ╚═══╝╚═╝  ╚═╝                    #
// #                                                                     #
// #/********************************************************************#

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <unistd.h>
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
#include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
#include <sys/iomgr.h>
#include <sys/iomsg.h>
// #include <sys/procfs.h>
// #include <sys/trace.h>
// #include <spawn.h>
#include <semaphore.h>

// build: qcc crash_namedsem.c -o crash_namedsem -Wall -g

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define mssleep(t) usleep((t * 1000UL))

#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
        exit(1);                                                               \
    }

int fill_coids(pid_t pid);


/********************************/
/* create_destroy_named_sem     */
/********************************/
void *create_destroy_named_sem(void *arg) {
    uint64_t loops = (uint64_t)arg;

    for (uint64_t u = 0; u < loops; u++) {
        sem_t *sem2 = sem_open("testing123", O_CREAT, S_IRWXU, 0);
        if (sem2 == SEM_FAILED) {
            puts("fail sem_open2\n");
            break;
        };

        sem_unlink("testing123");
        usleep(10);
        sem_close(sem2);
        usleep(10);
    }
    return NULL;
}

/********************************/
/* print_fdinfo                  */
/********************************/
void *print_fdinfo(void *arg) {
    uint64_t loops = (uint64_t)arg;

    for (uint64_t u = 0; u < loops; u++) {
        fill_coids(getpid());
        usleep(10);
    }
    return NULL;
}

sem_t *psem = NULL;

/*************************************/
/* create_destroy_anonymous_named_sem     */
/*************************************/
void *create_destroy_anonymous_named_sem(void *arg) {
    uint64_t loops = (uint64_t)arg;

    for (uint64_t u = 0; u < loops; u++) {
        while(psem != NULL);
        sem_t *sem2 = sem_open(NULL, O_ANON, S_IRWXU, 0);
        if (sem2 == SEM_FAILED) {
            puts("fail sem_open2\n");
            break;
        };

        psem = sem2;
        sem_close(sem2);
        usleep(10);
    }
    return NULL;
}


/*************************************/
/* dup_close_anonymous_named_sem     */
/*************************************/
void *dup_close_anonymous_named_sem(void *arg) {
    uint64_t loops = (uint64_t)arg;

    for (uint64_t u = 0; u < loops; u++) {
        while(psem == NULL);
        int duped_semfd = dup(psem->__u.__fd);

        if (duped_semfd > 0) {
            close(duped_semfd);
        }

        psem = NULL;
    }
    return NULL;
}



// Structure to hold the parsed options
typedef struct {
    int testcase_num;
    uint64_t loops;
} config_t;

/**
 * Parses -t <num> and -l <num> from command line.
 * Defaults: testcase = 0, loops = 1
 */
void parse_args(int argc, char *argv[], config_t *config) {
    int opt;

    // Initialize defaults
    config->testcase_num = 0;
    config->loops = ~0Ul;

    // ":" after a letter means that option requires an argument
    while ((opt = getopt(argc, argv, "t:l:")) != -1) {
        switch (opt) {
            case 't':
                config->testcase_num = atoi(optarg);
                break;
            case 'l':
                config->loops = strtoul(optarg, NULL, 0);
                break;
            case '?':
                // getopt prints its own error message by default
                fprintf(stderr, "Usage: %s [-t testcase_num] [-l loops]\n", argv[0]);
                exit(EXIT_FAILURE);
            default:
                exit(EXIT_FAILURE);
        }
    }
}


/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    config_t my_config;
    
    parse_args(argc, argv, &my_config);
    
    if (my_config.testcase_num == 1) {
        /**
         * one the creates, unlinks nad close a named sem
         * anopther thread does pidin fds: dup the sem's fd and call fdindo on it, 
         * closes the dup'd fd
         */
        pthread_t t1, t2;
        if (pthread_create(&t1, NULL, create_destroy_named_sem, (void*)my_config.loops) == -1) {
            failed(pthread_create, errno);
            return EXIT_FAILURE;
        }

        if (pthread_create(&t2, NULL, print_fdinfo, (void*)my_config.loops) == -1) {
            failed(pthread_create, errno);
            return EXIT_FAILURE;
        }
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
    }

    if (my_config.testcase_num == 2) {
        /**
         * one thread creates an anonymous sem, sends the fd over to another thread, 
         * close the semaphore
         * the other thread dups the fd, closes the dup'd fd
         */
        pthread_t t1, t2;
        if (pthread_create(&t1, NULL, create_destroy_anonymous_named_sem, (void*)my_config.loops) == -1) {
            failed(pthread_create, errno);
            return EXIT_FAILURE;
        }

        if (pthread_create(&t2, NULL, dup_close_anonymous_named_sem, (void*)my_config.loops) == -1) {
            failed(pthread_create, errno);
            return EXIT_FAILURE;
        }
        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
    }

    if (my_config.testcase_num == 3) {
        /**
         * create a semaphore, fork a child
         * child closes the sem first
         * check if the sem is closed after sem_close in the parent
         */
        sem_t *sem = sem_open(NULL, O_ANON, S_IRWXU, 0);
        if (sem == SEM_FAILED) {
            failed(sem_open, errno);
            return EXIT_FAILURE;
        };

        int cpid = fork();

        switch (cpid)
        {
        case -1:
            failed(fork, errno);
            return EXIT_FAILURE;
            break;
        case 0:
            /* child */
            if (sem_close(sem) == -1) {
                failed(sem_close, errno);
            }
            return EXIT_SUCCESS;
        default: {
            /* parent */
            struct _server_info sinfo;

            waitpid(cpid, NULL, 0);
            if (ConnectServerInfo(0, sem->__u.__fd, &sinfo) != sem->__u.__fd) {
                failed(ConnectServerInfo, errno);
            }
            if (sem_close(sem) == -1) {
                failed(sem_close, errno);
            }
            if (ConnectServerInfo(0, sem->__u.__fd, &sinfo) == sem->__u.__fd) {
                test_failed;
            }
            break;
        }
        }
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

struct coid_info {
    int fd;
    int pid;
    int chid;
    int ioflag;
    off_t offset;
    size_t size;
    char *name;
};
#define BUFINCR 10
#define PATHMGR_PID 1

/********************************/
/* fill_coids                   */
/********************************/
int fill_coids(pid_t pid) {
    struct coid_info *fdinfobuffer;
    int bcount, bmax;

    struct _server_info sinfo;
    int fd, fd2;
    io_dup_t msg;
    struct _fdinfo fdinfo;
    char path[512];
    int oldfd;

    fdinfobuffer = NULL;
    bcount = bmax = 0;

    for (fd = 0; (fd = ConnectServerInfo(pid, oldfd = fd, &sinfo)) >= 0 ||
                 ((oldfd & _NTO_SIDE_CHANNEL) == 0);
         ++fd) {
        int binsert;

        // Older versions of proc did not start at the first side channel
        if (fd < 0 ||
            (((fd ^ oldfd) & _NTO_SIDE_CHANNEL) && fd != _NTO_SIDE_CHANNEL)) {
            oldfd = _NTO_SIDE_CHANNEL;
            fd = _NTO_SIDE_CHANNEL - 1;
            continue;
        }

        if (bcount >= bmax) {
            struct coid_info *newfdbuffer;

            newfdbuffer =
                realloc(fdinfobuffer, (bmax + BUFINCR) * sizeof(*fdinfobuffer));
            if (newfdbuffer == NULL) {
                return 0;
            }
            bmax = bmax + BUFINCR;
            fdinfobuffer = newfdbuffer;
        }

        binsert = bcount++;

        memset(&fdinfobuffer[binsert], 0, sizeof(*fdinfobuffer));
        fdinfobuffer[binsert].fd = fd;
        fdinfobuffer[binsert].pid = sinfo.pid;
        fdinfobuffer[binsert].chid = sinfo.chid;
        fdinfobuffer[binsert].ioflag = -1;

        // If a connection points to itself or is a side channel,
        // it probably isn't a resmgr connection
        // however if it is a side channel to procnto, then it's likely a mount
        // point. even if it isn't a mount point, procnto will correctly handle
        // the _IO_DUP
        if ((sinfo.pid == pid) ||
            ((fd & _NTO_SIDE_CHANNEL) && (sinfo.pid != PATHMGR_PID))) {
            continue;
        }

        // Setup the flags for the duplicate connection.
        int attach_flags = (_NTO_COF_NOEVENT | _NTO_COF_CLOEXEC);
        // If the source connection is non-shared, ensure that its
        // duplicate is as well. This is required for in-kernel pipe
        // connections.
        attach_flags |= (sinfo.flags & _NTO_COF_NOSHARE);
        if ((fd2 = ConnectAttach(0, sinfo.pid, sinfo.chid, 0, attach_flags)) ==
            -1) {
            continue;
        }

        memset(&msg, 0, sizeof(msg));
        msg.i.type = _IO_DUP;
        msg.i.combine_len = sizeof msg;
        msg.i.info.pid = pid;
        msg.i.info.chid = sinfo.chid;
        msg.i.info.scoid = sinfo.scoid;
        msg.i.info.coid = fd;

        // We can't affort to block for long, .5 a second is all we will
        // tolerate. At the very least, when this happens we should still be
        // able to log the entry, just not with any name information.
        {
            struct sigevent event;
            uint64_t nsec;

            memset(&event, 0, sizeof(event));
            event.sigev_notify = SIGEV_UNBLOCK;
            nsec = 1 * 500000000L;

            TimerTimeout(CLOCK_REALTIME, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY,
                         &event, &nsec, NULL);
            if (MsgSendnc(fd2, &msg.i, sizeof msg.i, 0, 0) == -1) {
                ConnectDetach_r(fd2);
                continue;
            }
        }

        path[0] = 0;

        if (iofdinfo(fd2, _FDINFO_FLAG_LOCALPATH, &fdinfo, path,
                     sizeof(path)) == -1) {
            close(fd2);
            continue;
        }

        close(fd2);

        if (sinfo.pid == PATHMGR_PID && fdinfo.mode == 0) {
            fdinfobuffer[binsert].ioflag = fdinfo.flags;
            fdinfobuffer[binsert].offset = fdinfo.offset;
            fdinfobuffer[binsert].size = -1;
        } else {
            fdinfobuffer[binsert].ioflag = fdinfo.ioflag;
            fdinfobuffer[binsert].offset = fdinfo.offset;
            fdinfobuffer[binsert].size = fdinfo.size;
        }

        if (path[0] != 0) {
            printf("fd %d path %s\n", fd, path);
        }
        //fdinfobuffer[binsert].name = strdup(path);
    }

    free(fdinfobuffer);
    return 0;
}

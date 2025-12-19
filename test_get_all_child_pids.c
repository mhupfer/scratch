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
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
// #include <sys/memmsg.h>
#include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>
#include <pwd.h>

//build: qcc test_get_all_child_pids.c -o test_get_all_child_pids -g -Wall -I ~/mainline/stage/nto/usr/include/
//build: ntoaarch64-gcc test_get_all_child_pids.c -o test_get_all_child_pids -g -Wall -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define mssleep(t) usleep((t*1000UL))

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

#define NO_OF_CHILDS    42
pid64_t *get_all_childs(pid64_t pid, int *plen);
int get_pid64(pid_t pid, pid64_t *pid64);
int switch_to_user(const char *username);
int get_proc_info(pid64_t pid, debug_process_t *info);

#define PAGE_SIZE 4096

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    unsigned u;
    int listlen;
    pid64_t *childlist;

    //------------------------------------------------------------
    /* childs of procnto, should return some orphaned processes */
    childlist = get_all_childs(1, &listlen);
    
    if (!childlist) {
        test_failed;
    }

    // check against proc_info
    for (u = 0; u < listlen / sizeof(pid64_t); u++) {
        debug_process_t pinfo;

        if (get_proc_info(childlist[u], &pinfo) == EOK) {
            if (pinfo.parent != 1) {
                test_failed;
            }
        } else {
            test_failed;
        }
    }


    free(childlist);

    //------------------------------------------------------------
    /* childs of parent, pid 32 */
    childlist = get_all_childs(getppid(), &listlen);

    if (!childlist) {
        test_failed
    }

    for (u = 0; u < listlen / sizeof(pid64_t); u++) {
        if ((pid_t)childlist[u] == getpid()) {
            break;
        }
    }

    if (u == listlen) {
        /* prcoess pid is not in the parent's list */
        test_failed;
    }

    free(childlist);

    //------------------------------------------------------------
    /* childs of parent, pid 64 */
    pid64_t pid64;

    if (get_pid64(getppid(), &pid64) != EOK) {
        test_failed
    }

    childlist = get_all_childs(pid64, &listlen);

    if (!childlist) {
        test_failed
    }

    for (u = 0; u < listlen / sizeof(pid64_t); u++) {
        if ((pid_t)childlist[u] == getpid()) {
            break;
        }
    }

    if (u == listlen) {
        /* prcoess pid is not in the parent's list */
        test_failed;
    }

    free(childlist);

    //------------------------------------------------------------
    /* fork many children */

    pid_t *forklist = malloc(sizeof(pid_t) * NO_OF_CHILDS);

    if (!forklist) {
        test_failed;
    }

    for (u = 0; u < NO_OF_CHILDS; u++) {
        forklist[u] = fork();

        switch (forklist[u])
        {
        case -1:
            test_failed;
            break;
        case 0:
            /* child */
            alarm(8);
            pause();
            exit(0);
            break;
        default:
            break;
        }
    }


    childlist = get_all_childs(getpid(), &listlen);

    if (!childlist) {
        test_failed
    }

    if (listlen / sizeof(pid64_t) != NO_OF_CHILDS) {
        /* number of child is not correct */
        test_failed;
    }
    bool tfailed = false;

    for (u = 0; u < NO_OF_CHILDS; u++) {
        pid_t search_for = childlist[u];
        unsigned k;

        for (k = 0; k < NO_OF_CHILDS; k++) {
            if (search_for == forklist[k]) {
                break;
            }
        }

        if (k == NO_OF_CHILDS) {
            /* a child pid is missing */
            tfailed = true;
        }
    }

    free(childlist);

    //------------------------------------------------------------
    /* invalid buffer */
    {
        proc_get_all_child_pid_t msg = {
            .i.type = _PROC_GET_ALL_CHILD_PID,
            .i.pid = getpid()
        };

        void *buffer = mmap(NULL, 2*PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, NOFD, 0);
        munmap(buffer+PAGE_SIZE, PAGE_SIZE);

        int rc = MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), buffer+PAGE_SIZE-5*8,
                       PAGE_SIZE);

        if (rc != -EFAULT) {
            test_failed;
        }
    }

    //------------------------------------------------------------
    //no abilitx
    if (switch_to_user("qnxuser") == 0) {
        childlist = get_all_childs(getpid(), &listlen);

        if (childlist != NULL && errno != EPERM) {
            test_failed;
        }
    } else {
        test_failed;
    }

    for (u = 0; u < NO_OF_CHILDS; u++) {
        kill(forklist[u], SIGALRM);
    }

    if (tfailed) {
        test_failed;
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}



/********************************/
/* get_all_childs               */
/********************************/
pid64_t *get_all_childs(pid64_t pid, int *plen) {
    int pidlist_size = 256, rc;
    pid64_t *pidlist;
    
    proc_get_all_child_pid_t msg = {
        .i.type = _PROC_GET_ALL_CHILD_PID,
        .i.pid = pid
    };

    pidlist = malloc(pidlist_size);

    while (pidlist) {
        rc = MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), pidlist,
                       pidlist_size);

        if (rc > 0) {
            if (rc > pidlist_size) {
                free(pidlist);
                pidlist_size = rc;
                pidlist = malloc(pidlist_size);
            } else {
                *plen = rc;
                return pidlist;
            }
        } else if (rc == 0) {
            *plen = 0;
            free(pidlist);
            return NULL;
        } else {
            failed(MsgSend_r, -rc);
            errno = -rc;
            break;
        }
    }

    if (pidlist) {
        free(pidlist);
    }

    return NULL;
    
}

/********************************/
/* get_pid64                    */
/********************************/
int get_pid64(pid_t pid, pid64_t *pid64) {
    proc_getpid64_t msg;

    msg.i.type = _PROC_GETPID64;
    msg.i.pid = pid;

    int rc = -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), &msg.o, sizeof(msg.o));

    if (rc == EOK) {
        *pid64 = msg.o;
    } else {
        failed(MsgSend_r, rc);
    }

    return rc;
}

/********************************/
/* switch_to_user               */
/********************************/
int switch_to_user(const char *username) {
    struct passwd *pw = getpwnam(username);
    if (!pw) {
        perror("getpwnam");
        return -1;
    }

    if (seteuid(pw->pw_uid) != 0) {
        perror("seteuid");
        return -1;
    }

    // Optionally set real uid, gid
    if (setgid(pw->pw_gid) != 0) {
        perror("setgid");
        // return -1;
    }

    if (setuid(pw->pw_uid) != 0) {
        perror("setuid");
        // return -1;
    }

    return 0;
}

/********************************/
/* get_proc_info                */
/********************************/
int
get_proc_info(pid64_t pid, debug_process_t *info) {
    proc_debug_i_t i;
    i.type = _PROC_DEBUG;
    i.subtype = _PROC_DEBUG_PROC_INFO;
    i.pid = pid;

    int res = (int)-MsgSend_r(PROCMGR_COID, &i, sizeof(i), info, sizeof(*info));

    return res;
}

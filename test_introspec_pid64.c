#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <dirent.h>

#include <sys/procmsg.h>
// #include <sys/memmsg.h>
// #include <kernel/macros.h>

//build: qcc test_introspec_pid64.c -o test_introspec_pid64 -Wall -g -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

/*
#/bin/ksh
i=1
while (( i <= 30 ))
do
   sleep 400 &
   (( i+=1 ))
done

*/

int exec_test();
int test_as_non_root();
int get_pid64(pid_t pid, pid64_t *pid64);
int compare_pidlist_with_procfs(pid64_t *pidlist, int no_of_pids);


#define test_failed {\
    printf("Test FAILED at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    /* tests regardless of user */
    exec_test();

    /* 1000 should be the uid of qnxuser */
    if (setuid(1000) == 0) {
        exec_test();
    } else {
        failed(setuid, errno);
        test_failed
    }

    printf("Test PASSED\n");
    return EXIT_SUCCESS;
}


/********************************/
/* exec_test                 */
/********************************/
int exec_test() {

    /*------------ _PROC_GETPID64_GET_ALL ---------------*/

    proc_getallpid_t    msg;
    int                 pidlist_size = 128, rc;
    pid64_t             *pidlist;

    msg.i.type = _PROC_GET_ALL_PID;

    pidlist = malloc(pidlist_size);

    while (pidlist) {
        rc = MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), pidlist, pidlist_size);

        if (rc > 0) {
            if (rc > pidlist_size) {
                free(pidlist);
                pidlist_size = rc;
                pidlist = malloc(pidlist_size);
            } else {
                if (!compare_pidlist_with_procfs(pidlist, rc / sizeof(pid64_t))) {
                    test_failed;
                }
                // for (i = 0; i * sizeof(pid64_t) < rc; i++) {
                //     printf("pid %d pid64 %ld (0x%lx)\n", (pid_t)pidlist[i], pidlist[i], pidlist[i]);
                // }

                break;
            }
        } else {
            failed(MsgSend_r, -rc);
            test_failed;
        }

    }

    return 0;
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
/* compare_pidlist_with_procfs  */
/********************************/
int compare_pidlist_with_procfs(pid64_t *pidlist, int no_of_pids) {
    DIR             *dir;
    struct dirent   *ent;
    int             failed = 0, procfs_processes = 0;
    int             proc_pid, i, contained, rc;
    pid64_t         pid64;

    dir = opendir("/proc");

    if (dir != NULL) {
        ent = readdir(dir);

        while(ent && !failed) {
            proc_pid = atoi(ent->d_name);

            if (proc_pid != 0) {
                rc = get_pid64(proc_pid, &pid64);

                if (rc == EOK) {
                    procfs_processes++;

                    for (contained = 0, i = 0; i < no_of_pids; i++) {
                        if (pidlist[i] == pid64) {
                            contained = 1;
                            break;
                        }
                    }

                    if (!contained) {
                        failed = 1;
                    }
                } else {
                    failed(get_pid64, -rc);
                    failed = 1;
                }
            }
            
            ent = readdir(dir);
        }

        closedir(dir);

    } else {
        failed(opendir, errno);
        failed = 1;
    }

    if (procfs_processes != no_of_pids) {
        failed = 1;
    }

    return !failed;
}
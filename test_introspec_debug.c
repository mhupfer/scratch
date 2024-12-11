#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procmsg.h>
#include <unistd.h>

/* build: qcc test_introspec_debug.c -o test_introspec_debug -Wall -g -I ~/mainline/stage/nto/usr/include/ -lm
*/

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define test_failed                                                            \
    {                                                                          \
        printf("Test FAILED at %s, %d\n", __func__, __LINE__);                 \
        exit(1);                                                               \
    }

// print integer member
#define pim(struc, member) printf("%s = %d\n", #member, (struc).member)

pid64_t getpid64(pid_t pid);
// int stop_proc(pid64_t pid);
// int resume_proc(pid64_t pid);
// int dbg_attach(pid64_t pid);
// int dbg_detach(pid64_t pid);

int get_proc_info(pid64_t pid, debug_process_t *info);
int get_thread_status(pid64_t pid, unsigned tid, debug_thread_t *status);
int get_floatingpoint_regs(pid64_t pid, unsigned tid, debug_fpreg_t *regs);
int get_generalpurpose_regs(pid64_t pid, unsigned tid, debug_greg_t *regs);

#define CHILD_NO_OF_THREADS 4
#define THE_THREAD_WHO_DIES 3


/********************************/
/* sleepy_thread                */
/********************************/
void *sleepy_thread(void *a) {
    if (gettid() == THE_THREAD_WHO_DIES) {
        usleep(1000);
        return NULL;
    }

    usleep(100 * 1000);
    return NULL;
}


/********************************/
/* main							*/
/********************************/
int main(int argc, char *argv[]) {
    // pid64_t         mypid64;
    long res = 0;
    // char            path_buf[PATH_MAX];
    pid64_t pid64;
    proc_debug_t msg;

    /*-------------------------------------*/
    /* create a child and run tests on it */
    int child_pid = fork();

    switch (child_pid) {
    case -1:
        failed(fork, errno);
        exit(1);
        break;
    case 0: {
        /*
            child: create some threads, then sleep a while
            will be stopped by parent
            then the parent run several devctls on it
            finally the chuld is resumed
        */
        pthread_t handles[CHILD_NO_OF_THREADS];
        unsigned u;

        for (u = 0; u < CHILD_NO_OF_THREADS; u++) {
            //handles[0] gets tid 2
            res = pthread_create(&handles[u], NULL, sleepy_thread, NULL);

            if (res != EOK) {
                failed(pthread_create, res);
                exit(1);
            }
        }

        pthread_join(handles[THE_THREAD_WHO_DIES-2], NULL);
        usleep(100 * 1000);

        for (u = 0; u < CHILD_NO_OF_THREADS; u++) {
            if (u != THE_THREAD_WHO_DIES-2) {
                pthread_join(handles[u], NULL);
            }
        }

        exit(0);
        break;
    }
    default:
        break;
    }

    printf("parent %d child %d\n", getpid(), child_pid);

    pid64 = getpid64(child_pid);

    /* wait until the main thread of the child is in usleep() */
    for (msg.o_thread_status.state = 0;
         msg.o_thread_status.state != STATE_NANOSLEEP;) {
        res = get_thread_status(pid64, 1, &msg.o_thread_status);

        if (res != EOK) {
            failed(get_thread_status, res);
            exit(1);
        }

        usleep(1000);
    }

    if (kill(child_pid, SIGSTOP) == -1) {
        failed(kill, errno);
        exit(1);
    }

    /* check proc info, try child_pid */
    res = get_proc_info(pid64, &msg.o_proc_info);

    if (res != EOK) {
        test_failed;
    }

    if (msg.o_proc_info.num_threads != CHILD_NO_OF_THREADS) {
        test_failed;
    }

    /* check thread status */
    res = get_thread_status(pid64, 22, &msg.o_thread_status);

    if (res != ESRCH) {
        test_failed;
    }

    res = get_thread_status(pid64, 2, &msg.o_thread_status);

    if (res != EOK) {
        test_failed;
    }

    if (msg.o_thread_status.pid != child_pid || msg.o_thread_status.tid != 2) {
        test_failed;
    }


    /* test thread number hole detection */
    unsigned last_tid = 0;

    for (msg.o_thread_status.tid = 1;
         get_thread_status(pid64, msg.o_thread_status.tid,
                           &msg.o_thread_status) == EOK;
         msg.o_thread_status.tid++) {

        // printf("tid=%u state=%hhd\n", msg.o_thread_status.tid, msg.o_thread_status.state);

        if (msg.o_thread_status.tid == THE_THREAD_WHO_DIES) {
            test_failed;
        }
        last_tid = msg.o_thread_status.tid;
    }

    if (last_tid != CHILD_NO_OF_THREADS + 1) {
        test_failed;
    }

    int reglen = get_generalpurpose_regs(child_pid, 2, &msg.o_general_regs);

    if (reglen <= 0) {
        test_failed;
    }

    /* get regs for pid and pid64 and compare the result */
    proc_debug_t another_msg;

    int another_reglen = get_generalpurpose_regs(pid64, 2, &another_msg.o_general_regs);

    if (another_reglen <= 0) {
        test_failed;
    }

    if (reglen != another_reglen) {
        test_failed;
    }

    if (memcmp(&another_msg.o_general_regs, &msg.o_general_regs, reglen) != 0) {
        test_failed;
    }

    res = get_floatingpoint_regs(pid64, CHILD_NO_OF_THREADS + 1, &msg.o_fp_regs);

    if (res <= 0) {
        test_failed;
    }

    if (kill(child_pid, SIGCONT) == -1) {
        failed(kill, errno);
        exit(1);
    }

    waitpid(child_pid, NULL, 0);

    printf("Test PASSED\n");
    return EXIT_SUCCESS;
}


/********************************/
/* getpid64                     */
/********************************/
pid64_t getpid64(pid_t pid) {
    proc_getpid64_t pid64msg;

    pid64msg.i.type = _PROC_GETPID64;
    pid64msg.i.pid = pid;

    int rc = -MsgSend_r(PROCMGR_COID, &pid64msg.i, sizeof(pid64msg.i),
                        &pid64msg.o, sizeof(pid64msg.o));

    if (rc == EOK) {
        return pid64msg.o;
    } else {
        failed(MsgSend_r, rc);
        return -1;
    }
}



/********************************/
/* get_proc_info                */
/********************************/
int
get_proc_info(pid64_t pid, debug_process_t *info) {
    struct _proc_debug i;
    i.type = _PROC_DEBUG;
    i.subtype = _PROC_DEBUG_PROC_INFO;
    i.pid = pid;

    int res = (int)-MsgSend_r(PROCMGR_COID, &i, sizeof(i), info, sizeof(*info));

    return res;
}

/********************************/
/* get_thread_status            */
/********************************/
int
get_thread_status(pid64_t pid, unsigned tid, debug_thread_t *status) {
    struct _proc_debug i;
    i.type = _PROC_DEBUG;
    i.subtype = _PROC_DEBUG_THREAD_STATUS;
    i.pid = pid;
    i.tid = tid;

    int res =
        (int)-MsgSend_r(PROCMGR_COID, &i, sizeof(i),
                        status, sizeof(*status));

    return res;
}


/********************************/
/* get_generalpurpose_regs      */
/********************************/
int
get_generalpurpose_regs(pid64_t pid, unsigned tid, debug_greg_t *regs) {
    struct _proc_debug i;

    i.type = _PROC_DEBUG;
    i.subtype = _PROC_DEBUG_GET_GENERAL_REGS;
    i.pid = pid;
    i.tid = tid;

    int res = MsgSend_r(PROCMGR_COID, &i, sizeof(i), regs, sizeof(*regs));

    return res;
}

/********************************/
/* get_floatingpoint_regs       */
/********************************/
int
get_floatingpoint_regs(pid64_t pid, unsigned tid, debug_fpreg_t *regs) {
    struct _proc_debug i;

    i.type = _PROC_DEBUG;
    i.subtype = _PROC_DEBUG_GET_FP_REGS;
    i.pid = pid;
    i.tid = tid;

    int res = MsgSend_r(PROCMGR_COID, &i, sizeof(i), regs, sizeof(*regs));

    return res;
}



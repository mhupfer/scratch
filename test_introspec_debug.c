#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procmsg.h>
#include <unistd.h>

// build: qcc test_introspec_debug.c -o test_introspec_debug -Wall -g -I
// ~/mainline/stage/nto/usr/include/ -lm

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
int get_proc_info(pid64_t pid, proc_debug_t *msg);
int thread_status(pid64_t pid, int tid, proc_debug_t *msg);
// int set_current_thread(pid64_t pid, int tid);
int get_gp_regs(pid64_t pid, int tid, proc_debug_t *msg);
int get_fp_regs(pid64_t pid, int tid, proc_debug_t *msg);

/********************************/
/* sleepy_thread                */
/********************************/
void *sleepy_thread(void *a) {
    usleep(100 * 1000);
    return NULL;
}

#define CHILD_NO_OF_THREADS 3

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
            res = pthread_create(&handles[u], NULL, sleepy_thread, NULL);

            if (res != EOK) {
                failed(pthread_create, res);
                exit(1);
            }
        }

        usleep(100 * 1000);

        for (u = 0; u < CHILD_NO_OF_THREADS; u++) {
            pthread_join(handles[u], NULL);
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
        res = thread_status(pid64, 1, &msg);

        if (res != EOK) {
            failed(thread_status, res);
            exit(1);
        }

        usleep(1000);
    }

    if (kill(child_pid, SIGSTOP) == -1) {
        failed(kill, errno);
        exit(1);
    }

    /* check proc info, try child_pid */
    res = get_proc_info(pid64, &msg);

    if (res != EOK) {
        test_failed;
    }

    if (msg.o_proc_info.num_threads != CHILD_NO_OF_THREADS + 1) {
        test_failed;
    }

    // printf("\nPROCINFO\n");
    // printf("--------\n");
    // pim(msg.o_proc_info, parent);
    // pim(msg.o_proc_info, gid);
    // pim(msg.o_proc_info, uid);
    // pim(msg.o_proc_info, priority);
    // pim(msg.o_proc_info, num_threads);

    /* check thread status */
    res = thread_status(pid64, 22, &msg);

    if (res != ESRCH) {
        test_failed;
    }

    res = thread_status(pid64, 2, &msg);

    if (res != EOK) {
        test_failed;
    }

    if (msg.o_thread_status.pid != child_pid || msg.o_thread_status.tid != 2) {
        test_failed;
    }

    int reglen = get_gp_regs(child_pid, 2, &msg);

    if (reglen <= 0) {
        test_failed;
    }

    /* get regs for pid and pid64 and compare the result */
    proc_debug_t another_msg;

    int another_reglen = get_gp_regs(pid64, 2, &another_msg);

    if (another_reglen <= 0) {
        test_failed;
    }

    if (reglen != another_reglen) {
        test_failed;
    }

    if (memcmp(&another_msg.o_general_regs, &msg.o_general_regs, reglen) != 0) {
        test_failed;
    }

    res = get_fp_regs(pid64, CHILD_NO_OF_THREADS + 1, &msg);

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

// /********************************/
// /* dbg_attach                   */
// /********************************/
// int dbg_attach(pid64_t pid) {
//     proc_debug_t    msg;

//     memset(&msg.i, 0, sizeof (msg.i));
//     msg.i.type = _PROC_DEBUG;
//     msg.i.subtype = _PROC_DEBUG_ATTACH;
//     msg.i.pid = pid;

//     int res = -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), NULL, 0);

//     return res;
// }

// /********************************/
// /* dbg_detach                   */
// /********************************/
// int dbg_detach(pid64_t pid) {
//     proc_debug_t    msg;

//     memset(&msg.i, 0, sizeof (msg.i));
//     msg.i.type = _PROC_DEBUG;
//     msg.i.subtype = _PROC_DEBUG_DETACH;
//     msg.i.pid = pid;

//     int res = -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), NULL, 0);

//     return res;
// }

/********************************/
/* stop_proc                    */
/********************************/
// int stop_proc(pid64_t pid) {
//     proc_debug_t    msg;

//     memset(&msg.i, 0, sizeof (msg.i));
//     msg.i.type = _PROC_DEBUG;
//     msg.i.subtype = _PROC_DEBUG_STOP;
//     msg.i.pid = pid;

//     int res = -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), NULL, 0);

//     return res;
// }

/********************************/
/* resume_proc                  */
/********************************/
// int resume_proc(pid64_t pid) {
//     proc_debug_t    msg;

//     memset(&msg.i, 0, sizeof (msg.i));
//     msg.i.type = _PROC_DEBUG;
//     msg.i.subtype = _PROC_DEBUG_RUN;
//     msg.i.pid = pid;
// 	msg.i.run.flags |= _DEBUG_RUN_CLRFLT | _DEBUG_RUN_CLRSIG;

//     return -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), NULL, 0);
// }

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
int get_proc_info(pid64_t pid, proc_debug_t *msg) {
    memset(&msg->i, 0, sizeof(msg->i));
    msg->i.type = _PROC_DEBUG;
    msg->i.subtype = _PROC_DEBUG_PROC_INFO;
    msg->i.pid = pid;

    int res = -MsgSend_r(PROCMGR_COID, &msg->i, sizeof(msg->i),
                         &msg->o_proc_info, sizeof(msg->o_proc_info));

    return res;
}

/********************************/
/* thread_status                */
/********************************/
int thread_status(pid64_t pid, int tid, proc_debug_t *msg) {
    memset(&msg->i, 0, sizeof(msg->i));
    msg->i.type = _PROC_DEBUG;
    msg->i.subtype = _PROC_DEBUG_THREAD_STATUS;
    msg->i.pid = pid;
    msg->i.tid = tid;

    int res = -MsgSend_r(PROCMGR_COID, &msg->i, sizeof(msg->i),
                         &msg->o_thread_status, sizeof(msg->o_thread_status));

    return res;
}

/********************************/
/* thread_status                */
/********************************/
// int set_current_thread(pid64_t pid, int tid) {
//     proc_debug_t    msg;

//     memset(&msg.i, 0, sizeof (msg.i));
//     msg.i.type = _PROC_DEBUG;
//     msg.i.subtype = _PROC_DEBUG_CUR_THREAD;
//     msg.i.pid = pid;
// 	msg.i.tid = tid;

//     return -MsgSend_r(PROCMGR_COID, &msg.i, sizeof(msg.i), NULL, 0);
// }

/********************************/
/* get_gp_regs                  */
/********************************/
int get_gp_regs(pid64_t pid, int tid, proc_debug_t *msg) {
    memset(&msg->i, 0, sizeof(msg->i));
    msg->i.type = _PROC_DEBUG;
    msg->i.subtype = _PROC_DEBUG_GET_GENERAL_REGS;
    msg->i.pid = pid;
    msg->i.tid = tid;

    int res = MsgSend_r(PROCMGR_COID, &msg->i, sizeof(msg->i),
                        &msg->o_general_regs, sizeof(msg->o_general_regs));

    return res;
}

/********************************/
/* get_fp_regs                  */
/********************************/
int get_fp_regs(pid64_t pid, int tid, proc_debug_t *msg) {
    memset(&msg->i, 0, sizeof(msg->i));
    msg->i.type = _PROC_DEBUG;
    msg->i.subtype = _PROC_DEBUG_GET_FP_REGS;
    msg->i.pid = pid;
    msg->i.tid = tid;

    int res = MsgSend_r(PROCMGR_COID, &msg->i, sizeof(msg->i), &msg->o_fp_regs,
                        sizeof(msg->o_fp_regs));

    return res;
}

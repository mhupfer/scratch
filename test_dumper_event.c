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
#include <sys/procmsg.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>

//build: ntox86_64-gcc test_dumper_event.c -Wall -g -o test_dumper_event_x86_64 -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}

#define usleep_ms(t) usleep((t*1000UL))

#define DUMP_PULSE_CODE _PULSE_CODE_MINAVAIL

int register_dumper_event(int coid);
int cont_term(pid64_t pid, int status);
int spawn_crash();
int recv_pulse_to(int chid, struct _pulse *pulse, unsigned to_ms);
int get_proc_info(pid64_t pid, debug_process_t *info) ;
bool check_proc_exist(pid64_t pid);


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    int res;

    int my_chid = ChannelCreate(_NTO_CHF_PRIVATE);

    if (my_chid == -1) {
        failed(ChannelCreate, errno);
        return -1;
    }

    int my_coid = ConnectAttach(0, 0, my_chid, 0, 0);

    if (my_coid == -1) {
        failed(ConnectAttach, errno);
        return -1;
    }

    res = register_dumper_event(my_coid);

    if (res == -1) {
        system("slay dumper");
        usleep_ms(10);

        res = register_dumper_event(my_coid);

        if (res == -1) {
            failed(register_dumper_event, errno);
            return EXIT_FAILURE;
        }
    }

    /* check registering dumper twice */
    res = register_dumper_event(my_coid);

    if (res != -1 || errno != EBUSY) {
        test_failed;
    }

    /* check if the correct pid is received*/
    int crash_pid = spawn_crash();
    struct _pulse pulse;

    res = recv_pulse_to(my_chid, &pulse, 1000);

    if (res == EOK) {
        if ((uintptr_t)pulse.value.sival_ptr != (uintptr_t)crash_pid) {
            test_failed;
        }
    } else {
        failed(recv_pulse_to, errno);
        return EXIT_FAILURE;
    }

    /* the process should still exist */
    if (!check_proc_exist(crash_pid)) {
        test_failed;
    }

    if (cont_term(crash_pid, 1) != EOK) {
        failed(cont_term, errno);
        return EXIT_FAILURE;
    }

    usleep_ms(10);
    /* the process should have gone */
    if (check_proc_exist(crash_pid)) {
        test_failed;
    }

    /**
     * create 2 crashees, don't terminate them
     * exit this process, the 2 processes should  
     * be cleaned up and the dumper event beeing
     * unregistered
     */
    crash_pid = spawn_crash();
    int crash_pid2 = spawn_crash();

    res = recv_pulse_to(my_chid, &pulse, 1000);

    if (res != EOK) {
        failed(recv_pulse_to, errno);
        return EXIT_FAILURE;
    }

    if ((uintptr_t)pulse.value.sival_ptr != (uintptr_t)crash_pid) {
        test_failed;
    }

    res = recv_pulse_to(my_chid, &pulse, 1000);

    if (res != EOK) {
        failed(recv_pulse_to, errno);
        return EXIT_FAILURE;
    }

    if ((uintptr_t)pulse.value.sival_ptr != (uintptr_t)crash_pid2) {
        test_failed;
    }

    int child_pid = fork();

    switch (child_pid)
    {
    case -1:
        failed(fork, errno);
        return EXIT_FAILURE;
        break;
    case 0:
        break;
    default:
        /* parent just exits */
        return EXIT_SUCCESS;
        break;
    }

    usleep_ms(20);

    if (check_proc_exist(getppid())) {
        test_failed;
    }
    if (check_proc_exist(crash_pid)) {
        test_failed;
    }

    if (check_proc_exist(crash_pid2)) {
        test_failed;
    }

    res = register_dumper_event(my_coid);

    if (res == -1) {
        test_failed;
    }

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}


/********************************/
/* register_dumper_event        */
/********************************/
int register_dumper_event(int coid) {
    proc_coredump_t	msg;

    msg.i.type = _PROC_COREDUMP;
    msg.i.subtype = _PROC_COREDUMP_REGISTER_EVENT;
    SIGEV_PULSE_INIT(&msg.i.event, coid, -1, DUMP_PULSE_CODE, 0);
    SIGEV_MAKE_UPDATEABLE(&msg.i.event);

    if (MsgRegisterEvent(&msg.i.event, PROCMGR_COID) == -1) {
        exit(EXIT_FAILURE);
    }

    return MsgSend(PROCMGR_COID, &msg, sizeof(msg), NULL, 0);
}


/********************************/
/* cont_term                    */
/********************************/
int cont_term(pid64_t pid, int status) {
    proc_coredump_t	msg;
    msg.i.type = _PROC_COREDUMP;
    msg.i.subtype = _PROC_COREDUMP_CONT_TERM;
    msg.i.pid = pid;
    msg.i.flags = status == EOK ? FLAG_PROC_CORDEDUMP_SUCCESS : 0;
    
    return MsgSend(PROCMGR_COID, &msg, sizeof(msg), NULL, 0);
}


/********************************/
/* spawn_crash                  */
/********************************/
int spawn_crash() {
    char name[] = "crash_x86_64";
    char * __argv[] = {name, NULL};
    pid_t child_pid;

    int res = posix_spawnp(&child_pid, name, NULL, NULL, __argv, NULL);

    if (res == -1) {
        failed(posix_spawnp, errno);
        return -1;
    }

    return child_pid;
}


/********************************/
/* recv_pulse_to                */
/********************************/
int recv_pulse_to(int chid, struct _pulse *pulse, unsigned to_ms) {
    uint64_t to_ns = to_ms *1000000UL;

    if (TimerTimeout( CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &to_ns, NULL) == -1) {
        failed(TimerTimeout, errno);
        return -1;
    }

    return MsgReceivePulse(chid, pulse, sizeof(*pulse), NULL);
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
/* check_proc_exist             */
/********************************/
bool check_proc_exist(pid64_t pid) {
    debug_process_t dinfo;

    return get_proc_info(pid, &dinfo) == EOK;
}
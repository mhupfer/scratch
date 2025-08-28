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
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
#include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
#include <spawn.h>
#include <sys/ioctl.h>
//build: qcc test_dumper_error_reporting.c -o test_dumper_error_reporting -Wall -g -I ~/mainline/stage/nto/usr/include/


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

#define error_msg "get_proc_info() failed: No such process"
#define note_msg "fsync() error:Invalid argument"
#define info_msg "run fault pid"
#define dbg_msg "dumping to /dev/shmem/"

/********************************/
/* pipe_t                       */
/********************************/
typedef struct pipe_s {
    int fds[2];
} pipe_t;

/********************************/
/* close_pipe                   */
/********************************/
void close_pipe(pipe_t *p) {
    close(p->fds[0]);
    close(p->fds[1]);
}

/********************************/
/* read_pipe                    */
/********************************/
char *read_pipe(pipe_t *p) {
    unsigned count;

    if (ioctl(p->fds[0], FIONREAD, &count) == -1) {
        perror("ioctl");
        return NULL;
    }

    if (count > 0) {
        char *output = malloc(count);

        if (output) {
            if (read(p->fds[0], output, count) == -1) {
                failed(read, errno);
                free(output);
                return NULL;
            }
        }

        return output;
    }

    return NULL;
}

/********************************/
/* launch_process                */
/********************************/
int launch_process(char *args[], pipe_t *out, pipe_t * err) {

    if (pipe(out->fds) == -1) {
        perror("pipe(out)");
        return -1;
    }

    if (pipe(err->fds) == -1) {
        perror("pipe(err)");
        return -1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);

    // Redirect stdout (fd 1) to pipe write end
    posix_spawn_file_actions_adddup2(&actions, out->fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, err->fds[1], STDERR_FILENO);

    // Close fds not needed in child
    posix_spawn_file_actions_addclose(&actions, out->fds[0]);
    posix_spawn_file_actions_addclose(&actions, out->fds[1]);
    posix_spawn_file_actions_addclose(&actions, err->fds[0]);
    posix_spawn_file_actions_addclose(&actions, err->fds[1]);

    pid_t pid;

    if (posix_spawn(&pid, args[0], &actions, NULL, args, NULL) != 0) {
        perror("posix_spawnp");
        return -1;
    }

    posix_spawn_file_actions_destroy(&actions);

    return pid;
}


/********************************/
/* get_all_pids                 */
/********************************/
pid64_t * get_all_pids() {

    /*------------ _PROC_GETPID64_GET_ALL ---------------*/

    proc_getallpid_t msg;
    int pidlist_size = 256, rc;
    pid64_t *pidlist;

    msg.i.type = _PROC_GET_ALL_PID;

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
                return pidlist;
            }
        } else {
            failed(MsgSend_r, -rc);
            errno = -rc;
            break;
        }
    }

    return NULL;
}

/********************************/
/* CHECKMSG                     */
/********************************/
#define CHECKMSG(var, msg) ptr = strstr(output, msg);\
        if (var && !ptr) {\
            return false;\
        }\
\
        if (!var && ptr) {\
            return false;\
        }

/********************************/
/* check_output                 */
/********************************/
bool check_output(char *output, bool nothing, bool err, bool note, bool info, bool dbg) {
    char *ptr;

    if (nothing) {
        return output == NULL;
    } else {
        if (output == NULL) {
            return false;
        }

        CHECKMSG(err, error_msg);
        CHECKMSG(note, note_msg);
        CHECKMSG(info, info_msg);
        CHECKMSG(dbg, dbg_msg);
    }

    return true;
}

/********************************/
/* check_slog                   */
/********************************/
bool check_slog(bool nothing, bool err, bool note, bool info, bool dbg) {
    char *slog2_args[] = {"/proc/boot/slog2info", "-b", "dumper", NULL}; 
    pipe_t slout,slerr;

    pid_t slpid = launch_process(slog2_args, &slout, &slerr);

    if (slpid < 0) {
        failed(launch_process, errno);
        exit(EXIT_FAILURE);
    }

    mssleep(8);
    char *output = read_pipe(&slout);

    bool result = check_output(output, nothing, err, note, info, dbg);

    free(output);
    close_pipe(&slout);
    close_pipe(&slerr);

    return result;
}

/********************************/
/* test_error_daemon            */
/********************************/
void test_error_daemon() {
    // terminate running dumpers
    system("slay -f dumper");

    pipe_t dout, derr;
    pid_t pid;

    char *dumper_args[] = {"/system/bin/dumper", "-a", "-d", "/dev/shmem", NULL};

    pid = launch_process(dumper_args, &dout, &derr);

    if (pid < 0) {
        failed(launch_process, errno);
        exit(EXIT_FAILURE);
    }

    //clear slog
    system("slog2info -c > /dev/null");

    int procfd = open("/proc/dumper", O_RDWR);

    if (procfd == -1) {
        failed(open, errno);
        exit(EXIT_FAILURE);
    }

    char invalid_pid[] = "5555555\n";

    write(procfd, invalid_pid, strlen(invalid_pid));

    // if (write(procfd, invalid_pid, strlen(invalid_pid)) != -1) {
    //     failed(write, errno);
    //     return EXIT_FAILURE;
    // }

    if (!check_slog(false, true, false, false, false)) {
        test_failed;
    }

    char *output = read_pipe(&dout);
    if (!check_output(output, true, false, false, false, false)) {
        test_failed;
    }

    close(procfd);

    if (kill(pid, SIGTERM) == -1) {
        failed(kill, errno);
        exit(EXIT_FAILURE);
    }

    close_pipe(&dout);
    close_pipe(&derr);

}


/********************************/
/* test_error_daemon            */
/********************************/
void test_error_no_daemon() {
    // terminate running dumpers
    system("slay -f dumper");

    pipe_t dout, derr;
    pid_t pid;

    //clear slog
    system("slog2info -c > /dev/null");

    char *dumper_args[] = {"/system/bin/dumper", "-d", "/dev/shmem", "-p", "55555", NULL};

    pid = launch_process(dumper_args, &dout, &derr);

    if (pid < 0) {
        failed(launch_process, errno);
        exit(EXIT_FAILURE);
    }


    if (!check_slog(true, false, false, false, false)) {
        test_failed;
    }

    char *output = read_pipe(&derr);

    if (!check_output(output, false, true, false, false, false)) {
        test_failed;
    }

    free(output);

    if (kill(pid, SIGTERM) == -1) {
        failed(kill, errno);
        exit(EXIT_FAILURE);
    }

    close_pipe(&dout);
    close_pipe(&derr);

}



/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    /*
        Dumper can write to slog2 (daemon mode) or stderr
        The output should be either in slog or on console, not both
        It supports different verbosity levels
    */

    pid64_t * pidlist = get_all_pids();

    if (pidlist == NULL) {
        failed(get_all_pids, errno);
        return EXIT_FAILURE;
    }

    // the process to de dumped is the 3rd one
    char target_pid[32];
    snprintf(target_pid, sizeof(target_pid), "%u\n", (uint32_t)pidlist[3]);
    free(pidlist);

    test_error_daemon();
    test_error_no_daemon();

    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

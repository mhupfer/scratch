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
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
#include <sys/iomgr.h>
#include <sys/iomsg.h>
// #include <sys/procfs.h>
// #include <sys/trace.h>
// #include <spawn.h>
#include <semaphore.h>
#include <sys/procmgr.h>

// build: qcc test_fdinfo_xp.c -o test_fdinfo_xp -I ~/mainline/stage/nto/usr/include/ -Wall -g -L ~/mainline/stage/nto/x86_64/lib/

#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"
#define GRAY "\e[0;90m"
#define WARNING "\e[33m"

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define warning(f, e)                                                          \
    fprintf(stderr, WARNING "%s:%d: %s() failed: %s\n" ENDC, __func__,         \
            __LINE__, #f, strerror(e))

#define abort(f, e)                                                            \
    warning(f, e);                                                             \
    exit(EXIT_FAILURE)

#define mssleep(t) usleep((t * 1000UL))

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
        exit(1);                                                               \
    }

/********************************/
/* struct child_data            */
/********************************/
struct child_data {
    pid_t pid;
    pid_t target_pid;
    int exit_status;
};

/********************************/
/* proc_under_test              */
/********************************/
int
proc_under_test(int uid, struct child_data *d) {
    if (setgid(uid) == -1) {
        failed(setgid, errno);
        d->exit_status = EXIT_FAILURE;
        return EXIT_FAILURE;
    }

    if (setuid(uid) == -1) {
        failed(setuid, errno);
        d->exit_status = EXIT_FAILURE;
        return EXIT_FAILURE;
    }

    pause();

    d->exit_status = EXIT_SUCCESS;
    return d->exit_status;
}

/********************************/
/* tester                       */
/********************************/
int
tester(int uid, struct child_data *d) {
    int ret = procmgr_ability(
        0,
        PROCMGR_ADN_ROOT | PROCMGR_ADN_NONROOT | PROCMGR_AOP_ALLOW |
            PROCMGR_AID_XPROCESS_QUERY | PROCMGR_AOP_SUBRANGE,
        uid, uid, PROCMGR_AID_EOL);

    if (ret == -1) {
        failed(procmgr_ability, errno);
        d->exit_status = EXIT_FAILURE;
        return EXIT_FAILURE;
    }

    if (setgid(uid) == -1) {
        failed(setgid, errno);
        d->exit_status = EXIT_FAILURE;
        return EXIT_FAILURE;
    }

    if (setuid(uid) == -1) {
        failed(setuid, errno);
        d->exit_status = EXIT_FAILURE;
        return EXIT_FAILURE;
    }

    char p[256];
    d->exit_status = iofdinfo_xp(d->target_pid, 1, 0, NULL, p, sizeof(p)) == -1
                         ? EXIT_FAILURE
                         : EXIT_SUCCESS;
    return d->exit_status;
}

/********************************/
/* launch_child                   */
/********************************/
int
launch_child(int(f)(int, struct child_data *d), int uid, struct child_data *d) {
    int cpid = fork();

    switch (cpid) {
    case -1:
        failed(fork, errno);
        return -1;
        break;
    case 0:
        d->pid = getpid();
        exit(f(uid, d));
    default:
        return EOK;
    }
}

/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    int res;

    /**
     * Test the following as root
     * - stdout
     * - path of an opened file
     * - side channel connection
     * As non root, check
     * - iofdinfo_xp for permitted user
     * - iofdinfo_xp for non-permitted user
     */

    struct _fdinfo info;
    char path[PATH_MAX];

    res = iofdinfo_xp(getpid(), 1, 0, &info, path, sizeof(path));

    if (res == -1) {
        test_failed;
    } else {
        if (((strcmp("/dev/text", path) != 0) &&
             (strcmp("/dev/ttyp0", path) != 0) &&
             (strcmp("/dev/ser1", path) != 0)) != 0) {
            printf("path=%s\n", path);
            test_failed;
        }

        unsigned exepcted_mode = _S_IFCHR | S_IROTH | S_IWOTH | S_IRUSR |
                                 S_IWUSR | S_IRGRP | S_IWGRP;

        if ((info.mode & exepcted_mode) != exepcted_mode) {
            printf("mode = 0x%x expected 0x %x\n", info.mode, exepcted_mode);
            test_failed;
        }
    }

    int fd = open("/etc/group", O_RDONLY);

    if (fd == -1) {
        warning(open, errno);
    } else {

        res = iofdinfo_xp(getpid(), fd, 0, &info, path, sizeof(path));

        if (res == -1) {
            test_failed;
        } else {
            if (strcmp("/data/var/etc/group", path) != 0) {
                printf("path=%s excpected %s\n", path, "/etc/group");
                test_failed;
            }

            if ((info.mode &
                 (_S_IFREG | S_IROTH | S_IRUSR | S_IWUSR | S_IRGRP)) == 0) {
                test_failed;
            }
        }
    }

    /**
     * launch two processes under test as user ids 1001 and 1002
     * check iofdinfo_xp as root and 1001 against 1001 and 1002
     */
    // setup a shared mem for processes under test
    void *v =
        mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, NOFD, 0);

    if (v == MAP_FAILED) {
        abort(mmap, errno);
    }

    struct {
        struct child_data d1001;
        struct child_data d1002;
    } *pud = v;

    // launch 1st pud
    if (launch_child(proc_under_test, 1001, &pud->d1001) == -1) {
        abort(launch_child, errno);
    }

    // 2nd pud
    if (launch_child(proc_under_test, 1002, &pud->d1002) == -1) {
        abort(launch_child, errno);
    }

    // wait some time and check for launch errors
    mssleep(10);

    if (pud->d1001.exit_status == -1) {
        abort(pud->d1001.exit_status, pud->d1001.exit_status);
    }

    if (pud->d1002.exit_status == -1) {
        abort(pud->d1002.exit_status, pud->d1002.exit_status);
    }

    // test iofdinfo_xp as root
    res = iofdinfo_xp(pud->d1001.pid, 1, 0, &info, path, sizeof(path));

    if (res == -1) {
        test_failed;
    }

    res = iofdinfo_xp(pud->d1002.pid, 1, 0, &info, path, sizeof(path));

    if (res == -1) {
        test_failed;
    }

    // setup a shared mem for tester processes
    v = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, NOFD, 0);

    if (v == MAP_FAILED) {
        abort(mmap, errno);
    }

    struct {
        struct child_data t1001;
    } *testers = v;

    // check 1001 vs 1001
    testers->t1001.target_pid = pud->d1001.pid;

    if (launch_child(tester, 1001, &testers->t1001) == -1) {
        abort(launch_child, errno);
    }

    mssleep(10);
    waitpid(testers->t1001.pid, NULL, 0);

    if (testers->t1001.exit_status != EXIT_SUCCESS) {
        test_failed;
    }

    // check 1001 vs 1002
    testers->t1001.target_pid = pud->d1002.pid;

    if (launch_child(tester, 1001, &testers->t1001) == -1) {
        abort(launch_child, errno);
    }

    mssleep(10);
    waitpid(testers->t1001.pid, NULL, 0);

    if (testers->t1001.exit_status == EXIT_SUCCESS) {
        test_failed;
    }

    kill(pud->d1001.pid, SIGTERM);
    kill(pud->d1002.pid, SIGTERM);
    waitpid(pud->d1001.pid, NULL, 0);
    waitpid(pud->d1002.pid, NULL, 0);

    printf(PASSED "Test PASS" ENDC "\n");
    return EXIT_SUCCESS;
}

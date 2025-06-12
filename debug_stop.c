#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <devctl.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>

// ntoaarch64-gcc debug_stop.c -o debug_stop -Wall -g

static unsigned nthreads;

static void *
child_thread(void * const arg)
{
    for (;;);
    return NULL;
}

static int
child(void)
{
    // Spawn multiple threads.
    pthread_t *tids = calloc(nthreads, sizeof(pthread_t));
    for (unsigned i = 0; i < nthreads; i++) {
        int const rc = pthread_create(&tids[i], NULL, child_thread, NULL);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            return EXIT_FAILURE;
        }
    }

    sleep(3);
    return EXIT_SUCCESS;
}

static void *
preempter(void * const arg)
{
    uintptr_t runmask = 0x1;
    if (ThreadCtl(_NTO_TCTL_RUNMASK, (void *)runmask) == -1) {
        perror("ThreadCtl");
        exit(EXIT_FAILURE);
    }

    struct sched_param sched;
    if (SchedGet(0, 0, &sched) == -1) {
        perror("SchedGet");
        exit(EXIT_FAILURE);
    }

    sched.sched_priority++;
    if (SchedSet(0, 0, SCHED_NOCHANGE, &sched) == -1) {
        perror("SchedSet");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        usleep(10000);
    }

    return NULL;
}

int
main(int argc, char **argv)
{
    // Determine the number of processors in the system.
    long const ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpus < 0) {
        perror("sysconf");
        return EXIT_FAILURE;
    }

    nthreads = (unsigned)ncpus;

    // Create a process to debug.
    pid_t const pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        return child();
    }

    // Run at a high priority than the debugee.
    struct sched_param sched;
    if (SchedGet(0, 0, &sched) == -1) {
        perror("SchedGet");
        exit(EXIT_FAILURE);
    }

    sched.sched_priority++;
    if (SchedSet(0, 0, SCHED_NOCHANGE, &sched) == -1) {
        perror("SchedGet");
        exit(EXIT_FAILURE);
    }

    // Set thread affinity to a single core, such that it can be made floating
    // when acquiring a Muon mutex internally.
    uintptr_t runmask = 0x1;
    if (ThreadCtl(_NTO_TCTL_RUNMASK, (void *)runmask) == -1) {
        perror("ThreadCtl");
        return EXIT_FAILURE;
    }

    // Create a high-priority thread to preempt this one at regular intervals.
    int rc = pthread_create(NULL, NULL, preempter, NULL);
    if (rc != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        return EXIT_FAILURE;
    }

    // Attach as a debugger.
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/as", pid);

    int const dbg_fd = open(path, O_RDWR);
    if (dbg_fd == -1) {
        perror("open");
        kill(pid, SIGKILL);
        return EXIT_FAILURE;
    }

    for (;;) {
        usleep(100000);

        // Stop the debugged process.
        rc = devctl(dbg_fd, DCMD_PROC_STOP, NULL, 0, NULL);
        if (rc != 0) {
            fprintf(stderr, "devctl(STOP): %s\n", strerror(rc));
            kill(pid, SIGKILL);
            break;
        }

        for (pthread_t tid = 2; tid <= nthreads; tid++) {
            procfs_status status = { .tid = tid };
            rc = devctl(dbg_fd, DCMD_PROC_TIDSTATUS, &status, sizeof(status),
                        NULL);
            if (rc != 0) {
                fprintf(stderr, "devctl(TIDSTATUS): %s\n", strerror(rc));
                kill(pid, SIGKILL);
                break;
            }

            if (status.state == STATE_RUNNING) {
                printf("Thread %d is running\n", tid);
            }
        }

        // Continue the debugged process.
        procfs_run run = { 0 };
        rc = devctl(dbg_fd, DCMD_PROC_RUN, &run, sizeof(run), NULL);
        if (rc != 0) {
            fprintf(stderr, "devctl(RUN): %s\n", strerror(rc));
            kill(pid, SIGKILL);
            break;
        }

        for (pthread_t tid = 2; tid <= nthreads; tid++) {
            procfs_status status = { .tid = tid };
            rc = devctl(dbg_fd, DCMD_PROC_TIDSTATUS, &status, sizeof(status),
                        NULL);
            if (rc != 0) {
                fprintf(stderr, "devctl(TIDSTATUS): %s\n", strerror(rc));
                kill(pid, SIGKILL);
                break;
            }

            if ((status.state != STATE_RUNNING) &&
                (status.state != STATE_READY)) {
                printf("Thread %d is not running %d\n", tid, status.state);
            }
        }
    }

    return EXIT_SUCCESS;
}

/*
 * pulse_server.c
 *
 * This program, along with pulse_client.c, demonstrate pulses between a
 * server and a client using MsgDeliverEvent().
 *
 * The server attaches a name for the client to find using name_attach().
 * Since it is using name_attach(), it will have to handle some system pulses
 * and possibly messages, as well as the ones we're interested in.
 *
 * It will get a register message from the client.  This message will
 * contain an event to be delivered to the client.  It will also get a
 * regular pulse, and when that happens, it will deliver the event to
 * the client.
 *
 * When it gets a disconnect notification from the client, it needs to
 * clean up.
 *
 *  To test it, run it as follows:
 *    pulse_server
 *
 */

#include <errno.h>
#include <spawn.h>
#include <stdint.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

//build qcc spawn_hold_2.c -o spawn_hold_2 -Wall -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


int
main(int argc, char *argv[]) {
    pid_t child_pid;
    posix_spawnattr_t attr;

        if (argv[1] != NULL) {
            printf("Child gets executed\n");
            // usleep(1000 * 10);
            /* child */
            sigset_t mask;

            // Initialize empty signal set
            sigemptyset(&mask);

            // Add SIGCONT to the set
            sigaddset(&mask, SIGCONT);

            // Block SIGCONT
            if (sigprocmask(SIG_BLOCK, &mask, NULL) != 0) {
                failed("sigprocmask", errno);
                return 1;
            }

            if (kill(getpid(), SIGCONT) == -1) {
                failed(kill, errno);
                return 1;
            }

            if (sigprocmask(SIG_UNBLOCK, &mask, NULL) != 0) {
                failed("sigprocmask", errno);
                return 1;
            }


        return 0;
    }

    char *const _argv[] = {argv[0], "child", NULL};

    /* Initialize the spawn attributes structure. */
    if (posix_spawnattr_init(&attr) != 0) {
        failed(posix_spawnattr_init, errno);
        return -1;
    }
    /* Set the appropriate flag to make the changes take effect. */
    if (posix_spawnattr_setxflags(&attr, POSIX_SPAWN_HOLD) != 0) {
        failed(posix_spawnattr_setxflags, errno);
        return -1;
    }
    /* Spawn the child process. */
    if (posix_spawn(&child_pid, _argv[0], NULL, &attr,
                    _argv, NULL) != 0) {
        failed(posix_spawn, errno);
        return -1;
    }

    kill(child_pid, SIGCONT);
    
}

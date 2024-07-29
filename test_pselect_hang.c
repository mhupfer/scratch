#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/trace.h>
#include <sys/select.h>

static void
sigchld_handler(int sig)
{
	/* do nothing */
}

int main(int argn, char* argv[]) {
    /*
        Test if pselect hangs
        https://jira.bbqnx.net/browse/COREOS-126105
    */
    int pipe1[2];
    int pipe2[2];
    int rc;
    int verbosity = 0;

    if (argn > 1) {
        verbosity = atoi(argv[1]);
    }

    rc = pipe(pipe1);

    if (rc == -1) {
        perror("pipe(pipe1)");
        return EXIT_FAILURE;
    }

    rc = pipe(pipe2);

    if (rc == -1) {
        perror("pipe(pipe2)");
        return EXIT_FAILURE;
    }

    sigset_t blocking, original;

	sigemptyset(&blocking);
	sigaddset(&blocking, SIGCHLD);

    rc = sigprocmask(SIG_BLOCK, &blocking, &original);

    if (rc == -1) {
        perror("sigprocmask()");
        return EXIT_FAILURE;
    }

    struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigchld_handler;
	sigfillset(&sa.sa_mask);

	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction()");
        return EXIT_FAILURE;
    }

    int child_pid = fork();

    switch (child_pid)
    {
    case -1:
        perror("fork()");
        return EXIT_FAILURE;
    case 0:
    {
        /* child */
        struct _pulse  pulse;
        int val, ctrl_ch;

        ctrl_ch = ChannelCreate(0);
    
        if (ctrl_ch == -1) {
            perror("ChannelCreate()");
            return EXIT_FAILURE;
        }

        //Pulse 1
        rc = MsgReceivePulse(ctrl_ch, &pulse, sizeof(pulse), NULL);

        if (pulse.value.sival_int > 0) {
            usleep(1000 * pulse.value.sival_int);
        }


        if (verbosity > 0) {
            printf("child: write(pipe1)\n");
        }
        
        trace_logi(1, 1, 0);
        write(pipe1[1], &val, sizeof(val));

        //Pulse 2
        MsgReceivePulse(ctrl_ch, &pulse, sizeof(pulse), NULL);

        if (verbosity > 0) {
            printf("child: write(pipe2)\n");
        }

        trace_logi(1, 2, 0);
        write(pipe2[1], &val, sizeof(val));

        //Pulse 3
        MsgReceivePulse(ctrl_ch, &pulse, sizeof(pulse), NULL);

        if (pulse.value.sival_int > 0) {
            usleep(1000 * pulse.value.sival_int);
        }

        /* trigger SIGCHILD */
        if (verbosity > 0) {
            printf("child: exit\n");
        }

        trace_logi(1, 3, 0);
        return EXIT_SUCCESS;
    }
    default:
    {
        /* parent */
        fd_set  readfds;
        struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
        int val;
        int ctrl_con;

        usleep(100*1000);
       	
        ctrl_con = ConnectAttach(0, child_pid, 1, 0, 0);

        if (ctrl_con == -1) {
            perror("ConnectAttach()");
            kill(child_pid, SIGKILL);
            return EXIT_FAILURE;
        }

        FD_ZERO(&readfds);
        FD_SET(pipe1[0], &readfds);
        FD_SET(pipe2[0], &readfds);

        /* let the parent block and be unblocked by the child */
        MsgSendPulse(ctrl_con, 10, _PULSE_CODE_MINAVAIL, 100);

        if (verbosity > 0) {
            printf("parent: pselect()\n");
        }

        trace_logi(0, 1, 0);
        rc = pselect(pipe2[0] + 1, &readfds, NULL, NULL, &timeout, &original);

        switch (rc)
        {
        case -1:
            /* unexpected: interrupted by signal */
            printf("Test failed line no %d\n", __LINE__);
            kill(child_pid, SIGKILL);
            return EXIT_FAILURE;
        case 0:
            /* unexepcted: timeout */
            printf("Test failed line no %d\n", __LINE__);
            kill(child_pid, SIGKILL);
            return EXIT_FAILURE;
        default:
            /* expected:*/
            if (verbosity > 0) {
                printf("parent: read(pipe1)\n");
            }

            trace_logi(0, 2, 0);
            read(pipe1[0], &val, sizeof(val));
            break;
        }

        /* 
            1. let the child write to pipe2. Will create an obsolete SIGSELECT
            2. read the pipe
            4. let the child die
            3. see if pselect receives SIGCHLD
        */
        /* let the child write to pipe2 */
        MsgSendPulse(ctrl_con, 10, _PULSE_CODE_MINAVAIL, 0);
        /* let the child exit */
        MsgSendPulse(ctrl_con, 10, _PULSE_CODE_MINAVAIL, 0);
        usleep(100 * 1000);

        if (verbosity > 0) {
            printf("parent: read(pipe2)\n");
        }
        
        trace_logi(0, 3, 0);
        read(pipe2[0], &val, sizeof(val));

        FD_ZERO(&readfds);
        FD_SET(pipe1[0], &readfds);
        FD_SET(pipe2[0], &readfds);


        if (verbosity > 0) {
            printf("parent: pselect()\n");
        }
        
        trace_logi(0, 4, 0);
        rc = pselect(pipe2[0] + 1, &readfds, NULL, NULL, &timeout, &original);

        switch (rc)
        {
        case -1:
            /* expected: interrupted by signal */
            printf("SIGCHILD, unexpeced without fix, expected with fix\n");
            kill(child_pid, SIGKILL);
            break;
        case 0:
            /* timeout */
            printf("Timeout: expeced without fix, unexpected with fix\n");
            kill(child_pid, SIGKILL);
            break;
        default:
            /* unexpected:*/
            printf("Test failed line no %d\n", __LINE__);
            break;
        }
        break;
    }
    }

}
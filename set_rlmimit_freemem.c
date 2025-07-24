#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/neutrino.h>
#include <sys/procmsg.h>
#include <unistd.h>

//qcc set_rlmimit_freemem.c -o set_rlmimit_freemem -g -Wall


#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

void print_usage() {
	printf("set_rlmimit_freemem <pid>|'parent' limit (in promille)|'unlim'\n");
}

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    struct rlimit64 rl = {};
	int pid;

    if (argc > 2) {
		if (strcmp(argv[1], "parent") == 0) {
			pid = getppid();
		} else {
			pid = atoi(argv[1]);
		}

		if (strcmp(argv[2], "unlim") == 0) {
			rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
		} else {
        	rl.rlim_cur = rl.rlim_max = strtoul(argv[2], NULL, 0);
		}

		if (rl.rlim_cur == 0 || pid == 0) {
			print_usage();
			return 1;
		}
    } else {
		print_usage();
		return 1;
	}


	proc_resource_setlimit_t	msg;

	msg.i.type = _PROC_RESOURCE;
	msg.i.subtype = _PROC_RESOURCE_SETLIMIT;
	msg.i.pid = pid;
	msg.i.count = 1;
	msg.i.entry[0].resource = (unsigned)RLIMIT_FREEMEM;
	msg.i.entry[0].limit = rl;

	if (MsgSendnc(PROCMGR_COID, &msg.i, sizeof msg.i, NULL, 0) == -1) {
		failed(getrlimit, errno);
		return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}

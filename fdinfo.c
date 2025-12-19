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
#include <sys/neutrino.h>
#include <sys/iomsg.h>

//build: qcc fdinfo.c -o fdinfo -Wall -g

/********************************/
/* print_usage					*/
/********************************/
void print_usage(char *prgname) {
    printf("Check a process' file descriptors\n");
    printf("%s <pid>, <pid> > 1\n", prgname);
    exit(0);
}

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    pid_t pid;

    if (argc < 2) {
        print_usage(argv[0]);
    } else {
        pid = atoi(argv[1]);

        if (pid <= 1) {
            print_usage(argv[0]);
        }
    }

	struct _server_info	sinfo;
	char path[PATH_MAX];
	struct _fdinfo		fdinfo;

	for (int fd = 0;;fd++) {
		fd = ConnectServerInfo(pid, fd, &sinfo);

		if (fd < 0 || (fd & _NTO_SIDE_CHANNEL) != 0) {
			break;
		}

		int attach_flags = (_NTO_COF_NOEVENT | _NTO_COF_CLOEXEC);
		// If the source connection is non-shared, ensure that its
		// duplicate is as well. This is required for in-kernel pipe
		// connections.
		attach_flags |= (sinfo.flags & _NTO_COF_NOSHARE);
		int fd2 = ConnectAttach(0, sinfo.pid, sinfo.chid, 0, attach_flags);

		if(fd2 == -1) {
			continue;
		}

		io_dup_t		msg;
		memset(&msg, 0, sizeof(msg));
		msg.i.type = _IO_DUP;
		msg.i.combine_len = sizeof msg;
		msg.i.info.pid = pid;
		msg.i.info.chid = sinfo.chid;
		msg.i.info.scoid = sinfo.scoid;
		msg.i.info.coid = fd;

		if(MsgSendnc(fd2, &msg.i, sizeof msg.i, 0, 0) == -1) {
			ConnectDetach_r(fd2);
			continue;
		}

		path[0] = 0;

		if(iofdinfo(fd2, _FDINFO_FLAG_LOCALPATH, &fdinfo, path, sizeof(path)) == -1) {
			close(fd2);
			continue;
		}

		close(fd2);
		printf("pid %d fd %d path %s\n", pid, fd, path);
	}
    
    return EXIT_SUCCESS;
}


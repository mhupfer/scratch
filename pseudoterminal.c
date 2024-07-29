
#define __USE_XOPEN2KXSI
#define __USE_XOPEN_EXTENDED

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/neutrino.h>

//qcc -Wall -o pseudoterminal_qnx pseudoterminal.c -g -Wl,-L~/mainline/stage/nto/x86_64/lib/ -Wl,-L/home/mhupfer/mainline/qnxos/lib/c/lib/x86_64/so

char *get_realpath_of_controlling_tty() {
    int c_fd = open(ctermid(NULL), O_RDWR|O_NOCTTY);
    char *ret = ttyname(c_fd);
    close(c_fd);
    return ret;
}

void print_realpath_of_controlling_tty() {

    char * name = get_realpath_of_controlling_tty();

    if (name)
        printf("real path controlling tty %s\n", name);
    else
        printf("real path controlling tty %s\n", "NULL");
}

int main(int argc, char * argv[]) {
    char slave_name[256];
    int master_fd;

    int cpid = fork();

    if (cpid == -1) {
        printf("fork failed: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (cpid > 0) {
        waitpid(cpid, NULL, 0);
    }
    else {
        int pid = getpid();
        int shell_sid = getsid(pid);


        if (setsid() == -1) {
            printf("setsid failed: %s\n", strerror(errno));
        }

        printf("shell_sid %d sid %d pgid %d pid %d\n", shell_sid, getsid(pid), getpgid(pid), pid);
        
        print_realpath_of_controlling_tty();

        if (argc > 1) {
            printf("posix_openpt(O_RDWR|O_NOCTTY)\n");
            master_fd = posix_openpt(O_RDWR|O_NOCTTY);
        } else {
            printf("posix_openpt(O_RDWR)\n");
            master_fd = posix_openpt(O_RDWR);
        }

        struct _server_info sinfo;
        ConnectServerInfo(0, master_fd, &sinfo);
        printf("**** pid %d chid %d coid %d scoid %d\n", sinfo.pid, sinfo.chid, sinfo.coid, sinfo.scoid);

        printf("master fd %d path %s\n", master_fd, ttyname(master_fd));

        grantpt(master_fd);
        unlockpt(master_fd);
        ptsname_r(master_fd, slave_name, sizeof(slave_name));

        printf("slave name %s controlling terminal %s\n", slave_name, ctermid(NULL));

        int slave_fd = open(slave_name, O_RDWR/*|O_NOCTTY*/);

        if (slave_fd != -1) {
            printf("isatty(master %d)=%d isatty(slave %d)=%d\n", master_fd, isatty(master_fd), 
                slave_fd, isatty(slave_fd));

            close(slave_fd);
        } else {
            printf("Can't open slave dev %s: %s", slave_name, strerror(errno));
        }

        print_realpath_of_controlling_tty();
        char *rp = get_realpath_of_controlling_tty();

        if (!rp || strcmp(slave_name, rp) != 0)
        {
            printf("!!!!Pty is not the control process!!!!\n");
        }
    }
}
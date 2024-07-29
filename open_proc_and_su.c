#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/*
 * https://jira.bbqnx.net/browse/COREOS-125830
    opening /prroc/self/as
    fork()
    let the parent execute suid binary
    should not give the child access to as of the suid process
    which potentially runs with root privileges
*/
int main(int argc, char * argv[]) {
    const char proc_path[] = "/proc/self/as";
    int proc_fd = open(proc_path, O_RDWR);

    struct stat statbuf;

    if (stat(proc_path, &statbuf) == -1) {
        printf("stat(%s) failed: %s\n", proc_path, strerror(errno));
    } else {
        printf("st_mode = 0x%x\n", statbuf.st_mode);
    }

    if (proc_fd == -1) {
        printf("Cannot open %s: %s\n", proc_path, strerror(errno));
        return EXIT_FAILURE;
    }

    int child_pid = fork();

    switch (child_pid)
    {
    case -1:
        perror("fork() failed");
        break;
    case 0:
        /* child */
        sleep(3);
        char    buffer[256];

        int rc = read(proc_fd, buffer, sizeof(buffer));
        if (rc == -1)
            perror("child read failed");
        else
            printf("child read returned %d\n", rc);
        break;
    default:
        /* parent */
        char *argv [] =
        {
            "/system/xbin/su",
            "-c",
            "sleep 10",
            NULL
        };
        
        if (execv("/system/xbin/su", argv) == -1) {
            perror("execvp failed");
        }
        waitpid(child_pid, NULL, 0);
        break;
    }
}

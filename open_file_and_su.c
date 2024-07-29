#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/*
 * https://jira.bbqnx.net/browse/COREOS-125830
    opening a file which has write permissions for root only
    fork()
    let the parent execute suid binary -> becomes root rights
    should not give the child write perms to the file
*/
int main(int argc, char * argv[]) {

    if (argc < 2) {
        printf("Parameter 1 'file name' missing\n");
        return EXIT_FAILURE;
    }

    const char *proc_path = argv[1];
    int proc_fd = open(proc_path, O_RDONLY);

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
        char    buffer[256] = "corrupt";

        int rc = write(proc_fd, buffer, strlen(buffer));
        if (rc == -1)
            perror("child write failed");
        else
            printf("child write returned %d\n", rc);
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

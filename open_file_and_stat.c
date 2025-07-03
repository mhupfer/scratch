#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/trace.h>

//qcc open_file_and_stat.c -o open_file_and_stat -Wall -g

int main(int argc, char * argv[]) {

    if (argc < 2) {
        printf("Parameter 1 'file name' missing\n");
        return EXIT_FAILURE;
    }

    struct stat statbuf;
    const char *path = argv[1];
    
    trace_logf(42, "%s:%d", __func__, __LINE__);

    if (stat(path, &statbuf) == -1) {
        printf("stat(%s) failed: %s\n", path, strerror(errno));
        return EXIT_FAILURE;
    } else {
        printf("st_mode = 0x%x\n", statbuf.st_mode);
    }
    
    for (unsigned u = 0; u < 100000; u++) {
        int fd = open(path, O_RDONLY);

        if (fd == -1) {
            printf("open(%s) failed: %s\n", path, strerror(errno));
            return EXIT_FAILURE;
        }

        trace_logf(42, "%s:%d", __func__, __LINE__);

        if (fstat(fd, &statbuf) == -1) {
            printf("stat(%s) failed: %s\n", path, strerror(errno));
            break;
        }
        *((unsigned*)7) = 5;
        close(fd);
    }
}

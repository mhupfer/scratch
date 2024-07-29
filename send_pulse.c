#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/neutrino.h>

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


int main(int argn, char* argv[]) {
    int fd = open("/proc/dumper", O_RDWR);

    if (fd != -1) {
        if (MsgSendPulse(fd, 10, _PULSE_CODE_MAXAVAIL-1, 77) != 0) {
            failed(MsgSendPulse, errno);
        }
    } else {
        failed(open, errno);
    }
}
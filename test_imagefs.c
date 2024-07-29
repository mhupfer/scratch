#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/dcmd_all.h>
#include <sys/dcmd_blk.h>
#include <string.h>

int main(int argn, char* argv[]) {

    char*   fname;
    int     rc;

    if (argn > 1) {
        fname = argv[1];
    } else {
        fname = "/proc/boot/libc.so.6";
    }

    int fd = open(fname, O_RDONLY);

    if (fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    /* send DCMD_ALL_SETFLAGS without providing a reply buffer */
    struct emptymsg {
       io_devctl_t hdr;
       int  data;
    } msg;

	msg.hdr.i.type = _IO_DEVCTL;
	msg.hdr.i.combine_len = sizeof msg.hdr.i;
	msg.hdr.i.dcmd = DCMD_ALL_SETFLAGS;
	msg.hdr.i.nbytes = (uint32_t)sizeof(msg.data);
	msg.hdr.i.zero = 0;
    msg.data = O_NONBLOCK;

    rc = MsgSend(fd, &msg, sizeof(msg), NULL, 0);

    if (rc != 0) {
        perror("MsgSend(DCMD_ALL_SETFLAGS) failed");
        printf("No reply buffer test failed\n");
    } else {
        printf("No reply buffer test passed\n");
    }

    /* leak kernel data by providing a huge reply buffer */
    struct hugemsg {
       io_devctl_t hdr;
       char  data[1024];
    } hmsg;

	hmsg.hdr.i.type = _IO_DEVCTL;
	hmsg.hdr.i.combine_len = sizeof hmsg.hdr.i;
	hmsg.hdr.i.dcmd = DCMD_FSYS_STATVFS;
	hmsg.hdr.i.nbytes = (uint32_t)sizeof(hmsg.data);
	hmsg.hdr.i.zero = 0;
    memset(&hmsg.data, 0x42, sizeof(hmsg.data));

    rc = MsgSend(fd, &hmsg, sizeof(hmsg.hdr), &hmsg, sizeof(hmsg));

    if (rc != 0) {
        perror("MsgSend(DCMD_FSYS_STATVFS) failed");
    } else {
#if 0
        unsigned row, column, columns = 16;

        for (row = 0; row < sizeof(hmsg.data) / columns; row++) {
            for (column = 0; column < columns; column++) {
                printf("%2hhx ", hmsg.data[row * columns + column]);
            }
            printf("\t");
            for (column = 0; column < columns; column++) {
                printf("%c", hmsg.data[row * columns + column]);
            }
            printf("\n");
        }
#endif
        unsigned u;

        for(u = sizeof(struct __msg_statvfs); u < sizeof(hmsg.data); u++) {
            if (hmsg.data[u] != 0x42)
                break;
        }

        if (u == sizeof(hmsg.data))
            printf("data leak test passed\n");
        else
            printf("data leak test failed\n");
    }

    close(fd);
}
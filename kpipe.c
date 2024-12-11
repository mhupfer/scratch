#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/neutrino.h>
// #include <pthread.h>
#include <sys/kpipe.h>
#include <sys/iomsg.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>

//build: ntox86_64-gcc kpipe.c -o kpipe -Wall -g -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/x86_64/lib/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    struct _server_info     si;

    int chid = ChannelCreate(0);

    if (chid == -1) {
        failed(ChannelCreate, errno);
        exit(EXIT_FAILURE);
    }

    int coid_r = ConnectAttach(0, 0, chid, 0, _NTO_COF_NOSHARE);

    if (coid_r == -1) {
        failed(ConnectAttach, errno);
        exit(EXIT_FAILURE);
    }

    if (ConnectServerInfo(0, coid_r, &si) == -1) {
        failed(ConnectServerInfo, errno);
        exit(EXIT_FAILURE);
    }

    nto_pipe_cfg_t pipe_cfg = {0};

    pipe_cfg.pcfg_bufsz = 1024;
    pipe_cfg.pcfg_chunksz = 8;
    pipe_cfg.pcfg_type = _NTO_PIPE_CFG_SIMPLEX;
    pipe_cfg.pcfg_sourcebuf = 0;

    int pipe_id = PipeOpen(si.scoid, -1, &pipe_cfg, _IO_FLAG_RD);

    if (pipe_id == -1) {
        failed(PipeOpen, errno);
        exit(EXIT_FAILURE);
    }

    int coid_w = ConnectAttach(0, 0, chid, 0, _NTO_COF_NOSHARE);

    if (coid_w == -1) {
        failed(ConnectAttach, errno);
        exit(EXIT_FAILURE);
    }

    if (ConnectServerInfo(0, coid_w, &si) == -1) {
        failed(ConnectServerInfo, errno);
        exit(EXIT_FAILURE);
    }

    pipe_cfg.pcfg_sourcebuf = 1;

    if (PipeOpen(si.scoid, pipe_id, &pipe_cfg, _IO_FLAG_WR) == -1) {
        failed(PipeOpen, errno);
        exit(EXIT_FAILURE);
    }

    uint8_t rbuf[8] = {0};
    uint8_t wbuf[8];

    memset(wbuf, 0x42, sizeof(wbuf));

    if (write(coid_w, wbuf, sizeof(wbuf)) != sizeof(wbuf)) {
        failed(write, errno);
        exit(EXIT_FAILURE);
    }

    if (read(coid_r, rbuf, sizeof(rbuf)) != sizeof(rbuf)) {
        failed(read, errno);
        exit(EXIT_FAILURE);
    }

    if (memcmp(rbuf, wbuf, sizeof(wbuf)) == 0) {
        printf("Test PASSED\n");
    } else {
        printf("Test FAILED\n");
    }

    printf("parent fork %d", getpid());
    fflush(stdout);
    memset(wbuf, 0x44, sizeof(wbuf));
    int child_pid = fork();
    printf("forked %d", child_pid);

    switch (child_pid)
    {
    case -1:
        failed(fork, errno);
        exit(EXIT_FAILURE);
        break;
    case 0:
        printf("child %d", getpid());
        if (write(coid_w, wbuf, sizeof(wbuf)) != sizeof(wbuf)) {
            failed(write, errno);
            exit(EXIT_FAILURE);
        }

        for (;;)
            usleep(5000);
        break;
    default:
        printf("parent %d", getpid());

        if (read(coid_r, rbuf, sizeof(rbuf)) != sizeof(rbuf)) {
            failed(read, errno);
            exit(EXIT_FAILURE);
        }

        if (memcmp(rbuf, wbuf, sizeof(wbuf)) == 0) {
            printf("Fork-Test PASSED\n");
        } else {
            printf("Fork-Test FAILED\n");
        }
        waitpid(child_pid, NULL, 0);
        break;
    }
}

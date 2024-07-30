#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/iomsg.h>
#include <sys/dcmd_all.h>
#include <sys/dcmd_blk.h>
#include <string.h>
#include "sys/procmsg.h"

//ntox86_64-gcc  ~/scratch/test_procmgr_get_property.c -o test_procmgr_get_property -g -Wall -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


int check_pid64(int pid);
int check_proc_name(int pid);

/********************************/
/* main                         */
/********************************/
int main(int argn, char* argv[]) {

    char    *p;
    int     pid;

    if (argn < 2) {
        printf("test_procmgr_get_property <comma separated list of pids>");
    }

    p = strtok(argv[1], ",");

    while(p) {
        pid = atoi(p);

        if (pid != 0) {
            check_pid64(pid);
            check_proc_name(pid);
        }

        p = strtok(NULL, ",");
    }

    return EXIT_SUCCESS;
}


/********************************/
/* check_pid64                  */
/********************************/
int check_pid64(int pid) {
    proc_getpid64_t msg;

    memset(&msg.i, 0, sizeof(msg.i));
    msg.i.type = _PROC_GETPID64;
    msg.i.pid = pid;

    if (MsgSend(PROCMGR_COID, &msg.i, sizeof(msg.i), &msg, sizeof(msg)) == -1) {
        failed(MsgSend, errno);
        return -1;
    }

    proc_get_property_t prop;

    memset(&prop.i, 0, sizeof(prop.i));
    prop.i.type = _PROC_GET_PROPERTY;
    prop.i.subtype = _PROC_GETPROP_PID64;
    prop.i.pid = pid;

    if (MsgSend(PROCMGR_COID, &prop.i, sizeof(prop.i), &prop, sizeof(prop)) == -1) {
        failed(MsgSend, errno);
        return -1;
    }

    if (msg.o == prop.o.pid64) {
        printf("pid64 test for %d passed\n", pid);
    } else {
        printf("pid64 test for %d failed, %ld vs. %ld\n", pid, msg.o, prop.o.pid64);
        return -1;
    }
    
    return 0;
}


/********************************/
/* check_proc_name              */
/********************************/
int check_proc_name(int pid) {
    char    path[PATH_MAX];
    char    buf[PATH_MAX];
    char    another_buf[PATH_MAX];
    int     fd;

    sprintf(path, "/proc/%d/exefile", pid);

    fd = open(path, O_RDONLY);

    if (fd == -1) {
        failed(open, errno);
        return -1;
    }

    if (read(fd, buf, sizeof(buf)) == -1) {
        failed(read, errno);
        close(fd);
        return -1;
    }

    close(fd);

    proc_get_property_t *prop = (proc_get_property_t*)another_buf;

    memset(&prop->i, 0, sizeof(prop->i));
    prop->i.type = _PROC_GET_PROPERTY;
    prop->i.subtype = _PROC_GETPROP_NAME;
    prop->i.pid = pid;

    if (MsgSend(PROCMGR_COID, &prop->i, sizeof(prop->i), prop, sizeof(another_buf)) == -1) {
        failed(MsgSend, errno);
        return -1;
    }

    if (strcmp(buf, prop->o.name.data) == 0) {
        if (prop->o.name.name_len == strlen(prop->o.name.data)) {
            printf("name test for %d passed\n", pid);
        } else {
            printf("name test for %d failed reported len %u vs. %ld\n", pid, prop->o.name.name_len, strlen(prop->o.name.data));
        }
    } else {
        printf("name test for %d failed, %s vs. %s\n", pid, buf, prop->o.name.data);
    }

    return 0;
}

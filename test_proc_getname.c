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
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>
// #include <sys/memmsg.h>
#include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>

//build: qcc test_proc_getname.c -o test_proc_getname -Wall -g -I ~/mainline/stage/nto/usr/include/



#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

int get_process_name(pid64_t pid, char *buffer, size_t bsize);

/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    int pid = getpid();
    char *proc_name;

    if (argv[0][0] == '/')
        proc_name = &argv[0][1];
    else
        proc_name= argv[0];

    size_t dbg_name_len = strlen(proc_name);
    char    buffer[256];

    // enough space in the buffer
    memset(buffer, 'x', sizeof(buffer));
    int res = get_process_name(pid, buffer, sizeof(buffer));

    if (res != dbg_name_len) {
        test_failed;
    }

    if (strcmp(proc_name, buffer) != 0) {
        test_failed;
    }

    // insufficient space in the buffer
    memset(buffer, 'x', sizeof(buffer));
    res = get_process_name(pid, buffer, dbg_name_len / 2);

    if (res != dbg_name_len) {
        test_failed;
    }

    if (buffer[0] != '\0') {
        test_failed;
    }

    // zero space in the buffer
    memset(buffer, 'x', sizeof(buffer));
    res = get_process_name(pid, NULL, 0);

    if (res != dbg_name_len) {
        test_failed;
    }

    if (buffer[0] != 'x') {
        test_failed;
    }

    // 1 byte space in the buffer
    memset(buffer, 'x', sizeof(buffer));
    res = get_process_name(pid, buffer, 1);

    if (res != dbg_name_len) {
        test_failed;
    }

    if (buffer[0] != '\0') {
        test_failed;
    }


    printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}


/********************************/
/* get_process_name             */
/********************************/
int get_process_name(pid64_t pid, char *buffer, size_t bsize) {

    struct _proc_getname i = {
        .type = _PROC_GETNAME,
        .zero = 0,
        .pid = pid
    };

    return (int)MsgSend_r(PROCMGR_COID, &i, sizeof(i), (void *)buffer, bsize);
}
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
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>

//build: 

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 0
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif



/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    // printf(PASSED"Test PASS"ENDC"\n");
    uint64_t    result;
    uint64_t    op1;
    int64_t     op2;

    op1 = INT64_MAX;
    op2 = 15;
	if (__builtin_add_overflow(op1, op2, &result)) {
        failed(__builtin_add_overflow, EOVERFLOW);
    }
    printf("%lu + %ld = %lu (%ld)\n", op1, op2, result, (int64_t)result);

    op1 = 60;
    op2 = -15;
	if (__builtin_add_overflow(op1, op2, &result)) {
        failed(__builtin_add_overflow, EOVERFLOW);
    }
    printf("%lu + %ld = %lu (%ld)\n", op1, op2, result, (int64_t)result);

    op1 = 15;
    op2 = -30;
	if (__builtin_add_overflow(op1, op2, &result)) {
        failed(__builtin_add_overflow, EOVERFLOW);
    }
    printf("%lu + %ld = %lu (%ld)\n", op1, op2, result, (int64_t)result);

    op1 = (uint64_t)INT64_MAX + 10;
    op2 = INT64_MAX ;
	if (__builtin_add_overflow(op1, op2, &result)) {
        failed(__builtin_add_overflow, EOVERFLOW);
    }
    printf("%lu + %ld = %lu (%ld)\n", op1, op2, result, (int64_t)result);
}

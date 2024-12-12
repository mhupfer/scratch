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

//build: 

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

// #define PASSED  "\033[92m"
// #define FAILED "\033[91m"
// #define ENDC  "\033[0m"

// #define test_failed {\
//     printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
//     exit(1);\
// }


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

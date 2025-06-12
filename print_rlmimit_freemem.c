#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
// #include <time.h>
#include <string.h>
// #include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/neutrino.h>

//qcc print_rlmimit_freemem.c -o print_rlmimit_freemem -g -Wall


#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define NO_OF_MUTEX 2012

int create_robust_mutexes(pthread_mutex_t *mtx_array, int no_of_mtx, int *no_of_created_mutexes);
int destroy_mutexes(pthread_mutex_t *mtx_array, int no_of_mtx, int *no_of_destroyed_mutexes);
void *threaded_destroy(void *arg);
void *threaded_create(void *arg);
void threaded_test_init();

/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
	struct rlimit 			rl;

	/* check rlimit == unlimited */
	if (getrlimit(RLIMIT_FREEMEM, &rl) != 0) {
		failed(getrlimit, errno);
		return 1;
	} else {
		printf("RLIMIT_FREEMEM=%lu\n", rl.rlim_cur);
	}

}

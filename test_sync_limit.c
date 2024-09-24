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

//qcc test_sync_limit.c -o test_sync_limit -Wall -g  -L ~/mainline/stage/nto/x86_64/lib/ -I ~/mainline/stage/nto/usr/include/

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
    int                     rc, i;
    pthread_mutex_t         robust_mutex[NO_OF_MUTEX*2];
	struct rlimit 			rl;

	/* check rlimit == unlimited */
	if (getrlimit(RLIMIT_SYNC_NP, &rl) != 0) {
		failed(getrlimit, errno);
		return 1;
	} else {
		if (rl.rlim_cur != RLIM_INFINITY) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}
	}

	rc = create_robust_mutexes(robust_mutex, NO_OF_MUTEX*2, &i);

	if (rc != EOK || i != NO_OF_MUTEX*2) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	rc = destroy_mutexes(robust_mutex, NO_OF_MUTEX*2, &i);

	if (rc != EOK || i != NO_OF_MUTEX*2) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}


	/* set limit */
	rl.rlim_cur = NO_OF_MUTEX;
	rl.rlim_max = NO_OF_MUTEX;
	if (setrlimit(RLIMIT_SYNC_NP, &rl) != 0) {
		failed(setrlimit, errno);
		return 1;
	}

	/* check rlimit == NO_OF_MUTEX */
	if (getrlimit(RLIMIT_SYNC_NP, &rl) != 0) {
		failed(getrlimit, errno);
		return 1;
	} else {
		if (rl.rlim_cur != NO_OF_MUTEX) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}
	}

	/* ephemeral mtx should not be affected by the limit */
	for (i = 0; i < NO_OF_MUTEX + 1; i++) {
		rc = pthread_mutex_init(&robust_mutex[i], NULL);
		if (EOK != rc) {
			    failed(pthread_mutex_init, rc);
				return 1;
		}
	}

	if (rc != EOK || i != NO_OF_MUTEX + 1) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	rc = destroy_mutexes(robust_mutex, NO_OF_MUTEX + 1, &i);

	if (rc != EOK || i != NO_OF_MUTEX + 1) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	/* check if we hit the limit */
	rc = create_robust_mutexes(robust_mutex, NO_OF_MUTEX*2, &i);

	if (rc != ENOMEM || i != NO_OF_MUTEX) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	/* destroy half of the mutexes explicitly */
	rc = destroy_mutexes(robust_mutex, NO_OF_MUTEX / 2, &i);

	if (rc != EOK || i != NO_OF_MUTEX / 2) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	/* try to create NO_OF_MUTEX / 2 mutexes again 
		to prove half of them have been destroyed and
		the number got tracked correctly
	*/
	/* create NO_OF_MUTEX / 2 +1 mutexes, the last creation should fail */
	rc = create_robust_mutexes(robust_mutex, NO_OF_MUTEX/2 + 1, &i);


	if (rc != ENOMEM || i != NO_OF_MUTEX / 2) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	/* destroy all mutexes explicitly */
	rc = destroy_mutexes(robust_mutex, NO_OF_MUTEX, &i);

	if (rc != EOK || i != NO_OF_MUTEX) {
		printf("Test FAILED at line %d\n", __LINE__);
		return 1;
	}

	/*
		create the number of shared mutexes that fit in a page of memory
		free the page
		check if NO_OF_MUTEX robust mutextes can be created
	*/
	void *page = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, NOFD, 0);

	if (page != MAP_FAILED) {
		pthread_mutexattr_t     shared_attr;
    	pthread_mutex_t         *shared_mutexes = (pthread_mutex_t*)page;
		int						no_of_shared_mtx = 4096 / sizeof(*shared_mutexes);

	    rc = pthread_mutexattr_init(&shared_attr);
		if (EOK != rc) {
			failed(pthread_mutexattr_init, errno);
	        return 1;
		}

		rc = pthread_mutexattr_setpshared(&shared_attr, PTHREAD_PROCESS_SHARED);
		if (EOK != rc) {
			failed(pthread_mutexattr_setpshared, errno);
			return 1;
		}

		for (i = 0; i < no_of_shared_mtx; i++) {
	    	rc = pthread_mutex_init(&shared_mutexes[i], &shared_attr);
	    	if (EOK != rc) {
	    		    failed(pthread_mutex_init, rc);
	    			return 1;
	    	}
		}

		/* create robust mutexes again. Should fail if the number
		 	of robust mutexes + the number of shared mutexes hits the limit
		*/
		rc = create_robust_mutexes(robust_mutex, NO_OF_MUTEX, &i);

		if (rc != ENOMEM || i != NO_OF_MUTEX - no_of_shared_mtx) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}

		rc = destroy_mutexes(robust_mutex, i, &i);

		if (rc != EOK || i != NO_OF_MUTEX - no_of_shared_mtx) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}

		munmap(page, 4096);

		/* after unmapping it should be possible to create NO_OF_MUTEX
			robust mutexes again
		*/
		rc = create_robust_mutexes(robust_mutex, NO_OF_MUTEX, &i);

		if (rc != EOK || i != NO_OF_MUTEX) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}

		rc = destroy_mutexes(robust_mutex, i, &i);

		if (rc != EOK || i != NO_OF_MUTEX) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}

	} else {
		failed(mmap, errno);
		return 1;
	}

	/* 
		threaded mutex creation and destruction 
		concurrently create and destroy robust mutexes
		rlimit should be tracked corretly
	*/
	pthread_t 		h_destroyer, h_creator;
    uint64_t  		timeout = 5000000000LL;
    struct sigevent event;

    SIGEV_UNBLOCK_INIT (&event);

	for (i= 0; i < 1000; i++) {
		threaded_test_init();

		rc = pthread_create(&h_destroyer, NULL, threaded_destroy, NULL);

		if (rc != EOK) {
			failed(pthread_create, errno);
			return 1;
		}

		rc = pthread_create(&h_creator, NULL, threaded_create, NULL);

		if (rc != EOK) {
			failed(pthread_create, errno);
			return 1;
		}

		void * rc_destroy, *rc_create;
		pthread_join(h_creator, &rc_destroy);

	    TimerTimeout (CLOCK_MONOTONIC, _NTO_TIMEOUT_JOIN,
                  &event, &timeout, NULL);


		if (ETIMEDOUT == pthread_join(h_destroyer, &rc_create)) {
			failed(pthread_join, ETIMEDOUT);
			return 1;
		}

		if (rc_create != NULL || rc_destroy != NULL) {
			printf("Test FAILED at line %d\n", __LINE__);
			return 1;
		}
	}

	printf("Test PASSED\n");

}

/* globals shared between threads */
pthread_mutex_t  	shared_mutex_array[NO_OF_MUTEX];
int 				shared_current_no_of_mtx;


/********************************/
/* threaded_test_init			*/
/********************************/
void threaded_test_init() {
	shared_current_no_of_mtx = 0;
}

/********************************/
/* threaded_destroy				*/
/********************************/
void *threaded_destroy(void *arg) {
	int 	i = 0;
	long	rc;

	while(i < NO_OF_MUTEX) {
		if (i < shared_current_no_of_mtx) {
			rc = pthread_mutex_destroy(&shared_mutex_array[i]);
			if (EOK != rc) {
				failed(pthread_mutex_destroy, errno);
				return (void*)rc;
			}

			i++;
		} else {
			usleep(100);
		}
	}

	return (void*)rc;
}


/********************************/
/* threaded_create				*/
/********************************/
void *threaded_create(void *arg) {
	pthread_mutexattr_t     robust_attr;
	long					rc;

    rc = pthread_mutexattr_init(&robust_attr);
	if (EOK != rc) {
		failed(pthread_mutexattr_init, errno);
        return (void*)rc;
	}

	rc = pthread_mutexattr_setrobust(&robust_attr, PTHREAD_MUTEX_ROBUST);
	if (EOK != rc) {
		failed(pthread_mutexattr_setrobust, errno);
		return (void*)rc;
	}

    for (; shared_current_no_of_mtx < NO_OF_MUTEX; shared_current_no_of_mtx++) {
	    rc = pthread_mutex_init(&shared_mutex_array[shared_current_no_of_mtx], &robust_attr);
	    if (EOK != rc) {
			break;
	    }
    }

	if (shared_current_no_of_mtx != NO_OF_MUTEX) {
		rc = -1;
	}

	return (void*)rc;

}


/********************************/
/* create_robust_mutexes		*/
/********************************/
int create_robust_mutexes(pthread_mutex_t *mtx_array, int no_of_mtx, int *no_of_created_mutexes) {
	pthread_mutexattr_t     robust_attr;
	int						i, rc;

	*no_of_created_mutexes = -1;

    rc = pthread_mutexattr_init(&robust_attr);
	if (EOK != rc) {
		failed(pthread_mutexattr_init, errno);
        return rc;
	}

	rc = pthread_mutexattr_setrobust(&robust_attr, PTHREAD_MUTEX_ROBUST);
	if (EOK != rc) {
		failed(pthread_mutexattr_setrobust, errno);
		return rc;
	}

    for (i = 0; i < no_of_mtx; i++) {
	    rc = pthread_mutex_init(&mtx_array[i], &robust_attr);
	    if (EOK != rc) {
			break;
	    }
    }

	*no_of_created_mutexes = i;
	return rc;
}


/********************************/
/* destroy_mutexes				*/
/********************************/
int destroy_mutexes(pthread_mutex_t *mtx_array, int no_of_mtx, int *no_of_destroyed_mutexes) {
	int						i, rc;

	*no_of_destroyed_mutexes = -1;

	for (i = 0; i < no_of_mtx; i++) {
		rc = pthread_mutex_destroy(&mtx_array[i]);
		if (EOK != rc) {
			failed(pthread_mutex_destroy, errno);
			break;
		}
	}

	*no_of_destroyed_mutexes = i;
	return rc;
}

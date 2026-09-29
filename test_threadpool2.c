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
#include <sys/iofunc.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>
#define THREAD_POOL_HANDLE_T dispatch_t
#include <sys/dispatch.h>
#include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
#include <sys/trace.h>
#include <libgen.h>
#include <getopt.h>

//build: qcc test_threadpool2.c -o test_threadpool2 -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/x86_64/lib/ -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define mssleep(t) usleep((t*1000UL))

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#define test_abort(f,e) {\
    fprintf(stderr, FAILED"%s:%d: %s() failed: %s"ENDC"\n", __func__, __LINE__, #f, strerror(e));\
    exit(EXIT_FAILURE);\
}
#endif

typedef struct {
    unsigned   tpversion;
    unsigned    min_threads;
    unsigned    max_threads;
    unsigned    hysteresis;
} cmdline_options;

#define RESMGR_DEV "/dev/tpool2"
#define NO_OF_WORKERS   5


/********************************/
/* print_usage                  */
/********************************/
void print_usage() {
    printf("Siehe Quellcode\n");
}

/********************************/
/* parse_options                */
/********************************/
static int parse_options(int argc, char **argv, cmdline_options *opts) {
    int result = 0;
    opts->tpversion = 2;
    opts->min_threads = 2;
    opts->max_threads = 100;
    opts->hysteresis = 2;

    while (optind < argc) {
        int c = getopt(argc, argv, "v:m:M:h:?");

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'v':
                opts->tpversion = atoi(optarg);
                break;
            case 'm':
                opts->min_threads = atoi(optarg);
                break;
            case 'M':
                opts->max_threads = atoi(optarg);
                break;
            case 'h':
                opts->hysteresis = atoi(optarg);
                break;
            case '?':
                print_usage();
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "Unknown option: %c\n", c);
                result = -1;
                errno = EINVAL;
                break;
        }
    }

    if (opts->tpversion != 1 && opts->tpversion != 2) {
        result = EINVAL;
    }

    if (opts->min_threads == 0) {
        result = EINVAL;
    }

    if (opts->max_threads == 0) {
        result = EINVAL;
    }

    if (opts->hysteresis == 0) {
        result = EINVAL;
    }

    return result;
}

//------- resmgr -------
uint32_t    no_of_reads;

/********************************/
/* my_open                      */
/********************************/
int my_open(resmgr_context_t * const ctp, io_open_t * const msg, iofunc_attr_t * const attr, void * const extra) {
	return iofunc_open_default(ctp, msg, attr, extra);
}


/********************************/
/* my_read                      */
/********************************/
int my_read(resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb) {
    (void)__atomic_add_fetch(&no_of_reads, 1, __ATOMIC_RELAXED);
	return iofunc_read_default(ctp, msg, ocb);
    // _IO_SET_READ_NBYTES(ctp, 0);
}

//------- worker -------

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    int             allowed_workers; // Quota set by manager
    int             active_workers;  // Currently running workers
} worker_gate_t;

static worker_gate_t gate = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
    .allowed_workers = 0,
    .active_workers = 0
};


/********************************/
/* wait4gate                    */
/********************************/
void wait4gate(int *wstate) {
    pthread_mutex_lock(&gate.lock);

    if (*wstate == 0) {
        *wstate = 1;
    } else {
        gate.active_workers--;
    }

    
    if (gate.allowed_workers < 0) {
        pthread_mutex_unlock(&gate.lock);    
        pthread_exit(NULL);
    }

    // Block if active workers reach or exceed the allowed limit
    while (gate.active_workers > gate.allowed_workers) {
        pthread_cond_wait(&gate.cond, &gate.lock);

        if (gate.allowed_workers < 0) {
            pthread_mutex_unlock(&gate.lock);    
            pthread_exit(NULL);
        }
    }

    // Claim a slot
    gate.active_workers++;
    pthread_mutex_unlock(&gate.lock);    
}

/********************************/
/* update_gate                  */
/********************************/
void update_gate(int count) {
    pthread_mutex_lock(&gate.lock);
    
    gate.allowed_workers = count;

    // Wake up all sleeping workers to re-evaluate the new limit
    pthread_cond_broadcast(&gate.cond);
    
    pthread_mutex_unlock(&gate.lock);
}


/********************************/
/* open_read                    */
/********************************/
void *open_read(void *arg) {
    (void)pthread_setname_np(0, "open_read: open");
    int waitstate = 0;

    int fd = open(RESMGR_DEV, O_RDONLY);
    char buffer[32];
    
    (void)pthread_setname_np(0, "open_read: loop");

    if (fd != -1) {
        
        for (;;) {
            wait4gate(&waitstate);
            if (read(fd, &buffer, sizeof(buffer)) < 0) {
                trace_logf(42, "Read failed: %d", errno);
            }
        }
        close(fd);
    } else {
        trace_logf(42, "Open failed: %d", errno);
        failed(open, errno);
    }

    return NULL;
}

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    dispatch_t *dpp;
    static resmgr_connect_funcs_t connect_funcs;
    static resmgr_io_funcs_t io_funcs;
    resmgr_attr_t   rattr = {};
    iofunc_attr_t   attr;
    cmdline_options opts = {};

    (void)pthread_setname_np(0, "main");

    if (parse_options(argc, argv, &opts) != EOK) {
        print_usage();
        return EXIT_FAILURE;
    }

    int chid = ChannelCreate(0);

    if (chid == -1) {
        test_abort(ChannelCreate, errno);
    }

    // setup the thread pool
    if ((dpp = dispatch_create_channel(chid, 0)) == NULL) {
        test_abort(dispatch_create, errno);
    }

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS,
                     &io_funcs);
    connect_funcs.open = my_open;
    io_funcs.read = my_read;

    iofunc_attr_init(&attr, S_IRUSR | S_IRGRP, NULL, NULL);
    
    if (resmgr_attach(dpp, &rattr, RESMGR_DEV, _FTYPE_ANY, _RESMGR_FLAG_SELF,
                    &connect_funcs, &io_funcs, &attr) == -1 )
    {
    	test_abort("resmgr_attach", errno);
    }


    thread_pool2_t  *pool2 = NULL;
    thread_pool_t   *pool = NULL;

    if (opts.tpversion == 2) {
        thread_pool2_attr_t poolattr;     
        
        thread_pool2_attr_init(&poolattr, dpp, chid, opts.min_threads, opts.max_threads);
        poolattr.tid_name = "Malocher";
        poolattr.hysteresis = opts.hysteresis;
        
        pool2 = thread_pool2_create(&poolattr);
    
        if (pool2 == NULL) {
            test_abort(thread_pool2_create, errno);
        }

        int rc = thread_pool2_start(pool2);

        if (rc != EOK) {
            failed(thread_pool2_start, rc);
            return EXIT_FAILURE;
        }
    } else {
        thread_pool_attr_t	pool_attr = {};
	    pool_attr.handle = dpp;
	    pool_attr.context_alloc = (resmgr_context_t * (*)(dispatch_t *))dispatch_context_alloc;
	    pool_attr.block_func    = (resmgr_context_t * (*)(resmgr_context_t *))dispatch_block;
	    pool_attr.unblock_func  = (void (*)(resmgr_context_t *))dispatch_unblock;
	    pool_attr.handler_func  = (int (*)(resmgr_context_t *))dispatch_handler;
	    pool_attr.context_free  = (void (*)(resmgr_context_t *))dispatch_context_free;
	    pool_attr.tid_name      = "Malocher";
    	pool_attr.lo_water  = 2;
    	pool_attr.hi_water  = 3;
    	pool_attr.increment = 1;
    	pool_attr.maximum   = opts.max_threads;

        pool =  thread_pool_create (&pool_attr, 0);

        if (pool == NULL) {
            test_abort(thread_pool_create, errno);
        }

        int rc = thread_pool_start(pool);

        if (rc != EOK) {
            failed(thread_pool_start, rc);
            return EXIT_FAILURE;
        }
    }

    
    if (waitfor_attach(RESMGR_DEV, 5000) == -1) {
        fprintf(stderr, FAILED"Path %snot created"ENDC"\n", RESMGR_DEV);
        return EXIT_FAILURE;
    }

    pthread_t thandle[NO_OF_WORKERS];

    // create worker threads
    for (unsigned u = 0; u < NO_OF_WORKERS; u++) {
        int rc = pthread_create(&thandle[u], NULL, open_read, NULL);

        if (rc != EOK) {
            test_abort(pthread_create, rc);
        }
    }

    // work simulation
    update_gate(NO_OF_WORKERS);
    mssleep(200);
    update_gate(2);
    mssleep(200);
    update_gate(0);
    mssleep(200);
    update_gate(NO_OF_WORKERS);
    mssleep(200);

    // stop workers
    update_gate(-1);
    mssleep(200);

    for (unsigned u = 0; u < NO_OF_WORKERS; u++) {
        pthread_join(thandle[u], NULL);
    }

    if (opts.tpversion == 2) {
        thread_pool2_statistics_t stats;
        thread_pool2_get_statistics(pool2, &stats);
        thread_pool2_destroy(pool2);
        printf("Monitor wakeup send %u recv %u, Thread enter %u exit %u max %u\n",
            stats.monitor_send_wakeup, stats.monitor_recv_wakeup,
            stats.thread_enter, stats.thread_exit, stats.max_no_of_threads);
    } else {
        thread_pool_destroy(pool);
    }

    printf("Number of reads %u\n", no_of_reads);

    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

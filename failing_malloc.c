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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// build    qcc -shared -fPIC -o libfailing_malloc.so -g failing_malloc.c
// Use LD_PRELOAD=/data/home/root/libfailing_malloc.so pidin ar

static void* (*real_malloc)(size_t) = NULL;
static void* (*real_calloc)(size_t n, size_t size) = NULL;

static int  default_fail_probaility = 50; // Default to 50%
void*       fail_pc;
int         fail_index = -1;
bool        initialized = false;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/********************************/
/* caller_config_t              */
/********************************/
typedef struct {
    uintptr_t   pc_address;
    uint32_t    pc_idx;
    uint8_t     probability;
    uint8_t     orig_probability;
    uint8_t     next_probability;
} caller_config_t;

/********************************/
/* recorded_pc_t                */
/********************************/
typedef struct {
    uintptr_t   pc_address;
    uint32_t    count;
}   recorded_pc_t;

caller_config_t caller_config[16];
int             configured_callers;

recorded_pc_t   recorded_pcs[128];
unsigned        recorded_pcs_count;

/********************************/
/* parse_caller_pc_config       */
/********************************/
int parse_caller_pc_config(caller_config_t *configs, int max_items) {
    // 1. Fetch the environment variable
    char *env_val = getenv("CALLER_PC_CONFIG");
    if (env_val == NULL) {
        return 0;
    }

    if (strlen(env_val) >= 1024) {
        return -ENOMEM;
    }

    // Duplicate the string because strtok_r modifies the string in-place
    char pc_config[1024];
    strcpy(pc_config, env_val);

    int count = 0;
    char *outer_saveptr = NULL;
    
    // 2. Split into major blocks using the semicolon ';'
    char *block = strtok_r(pc_config, ";", &outer_saveptr);
    while (block != NULL && count < max_items) {
        
        char *inner_saveptr = NULL;
        // Split the block into tokens using the comma ','
        char *pcidx = strtok_r(block, ",", &inner_saveptr);
        char *prob = strtok_r(NULL, ",", &inner_saveptr);
        char *next_prob = strtok_r(NULL, ",", &inner_saveptr);

        // Ensure we got all 3 required fields for this block
        if (pcidx == NULL || prob == NULL || next_prob == NULL) {
            return -EINVAL;
        }


        if (pcidx[0] == '0' && pcidx[1] == 'x') {
            // it's a pc
            configs[count].pc_address = strtoul(pcidx, NULL, 0);
            configs[count].pc_idx = ~0U;
        } else {
            // index
            configs[count].pc_idx = strtoul(pcidx, NULL, 0);
            configs[count].pc_address = 0;
        }

        configs[count].orig_probability = configs[count].probability = strtoul(prob, NULL, 0);
        configs[count].next_probability = strtoul(next_prob, NULL, 0);
        
        count++;

        // Move to the next semicolon block
        block = strtok_r(NULL, ";", &outer_saveptr);
    }

    return count;
}

/********************************/
/* init_plugin                  */
/********************************/
__attribute__((constructor))
static void init_plugin() {
    srand((unsigned int)time(NULL));
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");

    // Read failure percentage from environment
    // determines the probabilty of malloc failing
    char *env_val = getenv("DEFAULT_FAIL_PROB");
    if (env_val) {
        int val = atoi(env_val);
        // Clamp value between 0 and 100
        if (val >= 0 && val <= 100) {
            default_fail_probaility = val;
        }
    }

    configured_callers = parse_caller_pc_config(caller_config, 16);
    initialized = true;
}


/********************************/
/* record_caller_pc             */
/********************************/
int record_caller_pc(uint64_t pc) {
    if (recorded_pcs_count >= 128) {
        return -1;
    }

    for (int u = 0; u < recorded_pcs_count; u++) {
        if (recorded_pcs[u].pc_address == pc) {
            recorded_pcs[u].count++;
            return u;
        }
    }

    recorded_pcs[recorded_pcs_count].pc_address = pc;
    recorded_pcs[recorded_pcs_count].count = 1;
    return recorded_pcs_count++;
}

/********************************/
/* should_fail                  */
/********************************/
bool should_fail(uintptr_t caller_pc, int index) {
    pthread_mutex_lock(&lock);

    for (unsigned u = 0; u < configured_callers; u++) {
        if ((caller_config[u].pc_address != 0 && caller_config[u].pc_address == caller_pc) ||
            (caller_config[u].pc_idx) != 0UL && caller_config[u].pc_idx == index) {
            /* we've seen this caller */
            bool fail = (rand() % 100) < caller_config[u].probability;
            caller_config[u].probability = caller_config[u].next_probability;
            pthread_mutex_unlock(&lock);
            return fail;
        }
    }

    return (default_fail_probaility != 0) && (rand() % 100) < default_fail_probaility;
}


/********************************/
/* malloc                       */
/********************************/
void* malloc(size_t size) {
    if (!real_malloc) {
        real_malloc = dlsym(RTLD_NEXT, "malloc");
    }

    if (!initialized) {
        return real_malloc(size);
    }

    uintptr_t caller_pc = (uintptr_t)__builtin_return_address(0);

    int idx = record_caller_pc(caller_pc);

    if (should_fail(caller_pc, idx)) {
        errno = ENOMEM;
        return NULL;
    } else {
        return real_malloc(size);
    }
}

/********************************/
/* calloc                       */
/********************************/
void * calloc(size_t n, size_t size)
{
    if (!real_calloc) {
        real_calloc = dlsym(RTLD_NEXT, "calloc");
    }

    if (!initialized) {
        return real_calloc(n, size);
    }

    uintptr_t caller_pc = (uintptr_t)__builtin_return_address(0);

    int idx = record_caller_pc(caller_pc);

    if (should_fail(caller_pc, idx)) {
        errno = ENOMEM;
        return NULL;
    } else {
        return real_calloc(n, size);
    }
}


/********************************/
/* print_config                 */
/********************************/
void print_config() {
    printf("\nAllocation interceptor lib configuration\n");
    printf("----------------------------------------\n");
    printf("Default allocation fail probability: %u\n", default_fail_probaility);

    if (configured_callers > 0) {
        printf("Caller pc configuration:\n");
        for (unsigned u = 0; u < configured_callers; u++) {
            if (caller_config[u].pc_idx != ~0U) {
                printf("\tIndex %u probability %u next probability %u\n",
                       caller_config[u].pc_idx,
                       caller_config[u].orig_probability,
                       caller_config[u].next_probability);
            } else {
                printf("\tPc 0x%lx probability %u next probability %u\n",
                       caller_config[u].pc_address,
                       caller_config[u].orig_probability,
                       caller_config[u].next_probability);
            }
        }
    }
}

/********************************/
/* deinit                       */
/********************************/
__attribute__((destructor))
void deinit() {
    print_config();
    printf("malloc()/calloc() calls:\n");
    printf("------------------------\n");

    pthread_mutex_lock(&lock);

    for (unsigned u = 0; u < recorded_pcs_count; u++) {
        printf("Call %u pc 0x%lx count %u\n", u, recorded_pcs[u].pc_address,
               recorded_pcs[u].count);
    }

    pthread_mutex_unlock(&lock);
}

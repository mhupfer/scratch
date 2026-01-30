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
#include <sys/neutrino.h>
// #include <sys/resmgr.h>
// #include <sys/iofunc.h>
// #include <sys/dispatch.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
#include <spawn.h>
#include <sys/introspection.h>
#include <setjmp.h>
#include <private/memtag.h>

//build: ntoaarch64-gcc -march=armv8-a+memtag mte.c -o mte -Wall -g -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/aarch64le/lib/ -lintrospection

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define mssleep(t) usleep((t*1000UL))

void print_mte_regs();
char** create_memtag_environment(void);

#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#if 0
#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

#if 0
/**
 * Set the allocation tag in memory for a tagged pointer
 * Uses the STG (Store Tag) instruction to write the tag to memory
 * 
 * @param ptr Tagged pointer
 */
static inline void mte_set_allocation_tag(void *ptr) {
#ifdef __aarch64__
    __asm__ volatile(
        "stg %0, [%0]"
        :
        : "r" (ptr)
        : "memory"
    );
#else
    // No-op on non-ARM64 platforms
    (void)ptr;
#endif
}

/**
 * Load the allocation tag from memory into a pointer
 * Uses the LDG (Load Tag) instruction
 * 
 * @param ptr Pointer to the address
 * @return Pointer with tag loaded from memory
 */
static inline void* mte_load_allocation_tag(void *ptr) {
#ifdef __aarch64__
    void *tagged_ptr;
    __asm__ volatile(
        "ldg %0, [%1]"
        : "=r" (tagged_ptr)
        : "r" (ptr)
    );
    return tagged_ptr;
#else
    return ptr;
#endif
}

#endif

sigjmp_buf jmpbuf;
siginfo_t g_siginfo;

/********************************/
/* segv_handler                 */
/********************************/
static void segv_handler(int sig, siginfo_t *si, void *ctx)
{
    memcpy(&g_siginfo, si, sizeof(g_siginfo));
    siglongjmp(jmpbuf, 1);
}

/********************************/
/* create_tagged_ptr            */
/********************************/
void *create_tagged_ptr(void *untagged, size_t size) {
    void *tagged_ptr;

    __asm__ volatile(
        "irg %0, %1"
        : "=r" (tagged_ptr)
        : "r" (untagged)
    );

    char *p = (char*)tagged_ptr;
    size_t granules = size / 16;  // MTE operates on 16-byte granules
    
    for (size_t i = 0; i < granules; i++) {
        __asm__ volatile(
            "stzg %0, [%0]"
            :
            : "r" (p)
            : "memory"
        );
        p += 16;
    }

    return tagged_ptr;
}

#define TCPASSED(no) printf(PASSED"Testcase %u: Passed"ENDC"\n", (no))
#define TCFAILED(no) printf(FAILED"Testcase %u: Failed"ENDC"\n", (no))

#define BUF_SIZE        512

/********************************/
/* execute_mte_tests            */
/********************************/
int execute_mte_tests() {
    unsigned test_case_no = 1;
    bool    expect_exception = false;
    uint32_t *vtagged = NULL;

    printf("Allocate some mem\n");
    char *p = malloc(BUF_SIZE);

    if (p == NULL) {
        failed(malloc, ENOMEM);
        return -1;
    }

    signal(SIGSEGV, (void (*)(int))segv_handler);
    
    if (sigsetjmp(jmpbuf, 1) != 0) {
        if (expect_exception) {
            TCPASSED(test_case_no);
        } else {
            TCFAILED(test_case_no);
        }
        test_case_no++;
    }


    // access last element
    if (test_case_no == 1) {
        printf("Testcase %u Access last element: ", test_case_no);
        expect_exception = false;
        p[BUF_SIZE - 1] = 1;
        TCPASSED(test_case_no);
        test_case_no++;
    }

    //write beyond
    if (test_case_no == 2) {
        printf("Testcase %u Access beyond allocated: ", test_case_no);
        expect_exception = true;
        p[BUF_SIZE + 80] = 2;
        TCFAILED(test_case_no);
        test_case_no++;
    }

    //write before
    if (test_case_no == 3) {
        printf("Testcase %u Access before allocated: ", test_case_no);
        expect_exception = true;
        *(p-1) = 2;
        TCFAILED(test_case_no);
        test_case_no++;
    }

    // use after free
    if (test_case_no == 4) {
        free(p);
        printf("Testcase %u Use after free: ", test_case_no);
        expect_exception = true;
        p[BUF_SIZE - 1  ] = 1;
        TCFAILED(test_case_no);
        test_case_no++;
    }

    if (test_case_no == 5) {
        printf("Testcase %u Explicit tagging: ", test_case_no);
        expect_exception = true;

        void *v = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_MEMTAG, MAP_ANON | MAP_SHARED, NOFD, 0);

        if (v == MAP_FAILED) {
            failed(mmap, errno);
            return -1;
        }

        tag_range(v, 256, (void**)&vtagged);


        printf("Tagged ptr 0x%p ", vtagged);
        printf("%u ", vtagged[256 / 4 - 4]);
        printf("%u ", vtagged[256 / 4]);
        TCFAILED(test_case_no);
        test_case_no++;
    }

    if (test_case_no == 6) {
        expect_exception = false;
        printf("Testcase %u Read Memory via Introspection API: ", test_case_no);
        char buf[256];

        memset(buf, 0, sizeof(buf));
        memset(vtagged, 0xfe, sizeof(buf));

        int rc = introspection_read_process_memory(0, (uintptr_t)vtagged, buf, sizeof(buf));

        if (rc < 0) {
            failed(introspection_read_process_memory, -rc);
            TCFAILED(test_case_no);
        } else {
            if (memcmp(buf, vtagged, sizeof(buf)) == 0) {
                TCPASSED(test_case_no);
            } else {
                TCFAILED(test_case_no);
            }
        }
        test_case_no++;
    }

    if (test_case_no == 7) {
        expect_exception = false;
        printf("Testcase %u Read a file from a resource daemon: ", test_case_no);

        p = malloc(BUF_SIZE);

        if (p == NULL) {
            failed(malloc, ENOMEM);
            return -1;
        }

        memset(p, 0xfe, BUF_SIZE);

        int fd = open("/system/etc/ssh/sshd_config", O_RDONLY);

        if (fd < 0) {
            failed(open, errno);
            TCFAILED(test_case_no);
        } else {
            int res = read(fd, p, BUF_SIZE);

            if (res > 0) {
                if (strstr(p, "AuthorizedKeysFile") != 0) {
                    TCPASSED(test_case_no);
                } else {
                    TCFAILED(test_case_no);
                }
            } else {
                failed(read, errno);
                TCFAILED(test_case_no);
            }

            close(fd);
        }

        free(p);
        test_case_no++;
    }
    return EOK;
}


/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    char *arg1 = "MTE";

    if (argv[1] != NULL) {
        /* child spawned with activated MTE */
        if (strcmp(argv[1], "MTE") == 0) {
            return execute_mte_tests();
        }

        if (strcmp(argv[1], "PRINT_REGISTERS") == 0) {
            print_mte_regs();
            return EXIT_SUCCESS;
        }

        if (strcmp(argv[1], "regs") == 0) {
            arg1 = "PRINT_REGISTERS";
        }

    }    

    posix_spawnattr_t   attr;
    int                 child_pid;
    char exe_name[PATH_MAX] = "/";

    char *const _argv[] = {exe_name, arg1, NULL};

    if (introspection_get_process_name(getpid(), exe_name + 1, sizeof(exe_name) - 1) < 0)  {
        strlcat(exe_name, argv[0], sizeof(exe_name) - 1);
    }

    /* Initialize the spawn attributes structure. */
    if (posix_spawnattr_init(&attr) != 0) {
        failed(posix_spawnattr_init, errno);
        return -1;
    }
    /* Set the appropriate flag to make the changes take effect. */
    if (posix_spawnattr_setxflags(&attr, POSIX_SPAWN_MEMTAG) != 0) {
        failed(posix_spawnattr_setxflags, errno);
        return -1;
    }

    char **new_env = create_memtag_environment();
    if (new_env == NULL) {
        failed(create_memtag_environment, errno);
        return -1;
    }

    /* Spawn the child process. */
    if (posix_spawn(&child_pid, _argv[0], NULL, &attr,
                    _argv, new_env) != 0) {
        failed(posix_spawn, errno);
        return -1;
    }

    kill(child_pid, SIGCONT);

    // printf(PASSED"Test PASS"ENDC"\n");
    return EXIT_SUCCESS;
}

#define SCTLR_ATA0_BIT   42
#define SCTLR_ATA_BIT    43
#define SCTLR_TCF0_SHIFT 38

// Bit position in TCR_EL1
#define TCR_TBI0_BIT 37
#define TCR_TBI1_BIT 38

/**
 * Read SCTLR_EL1 system register
 * Note: This requires EL1 privileges (kernel mode)
 */
static inline uint64_t read_sctlr_el1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r" (val));
    return val;
}

/**
 * Read TCR_EL1 system register
 */
static inline uint64_t read_tcr_el1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, tcr_el1" : "=r" (val));
    return val;
}

/**
 * Read GCR_EL1 system register
 */
static inline uint64_t read_gcr_el1(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, gcr_el1" : "=r" (val));
    return val;
}

/**
 * Extract a bit field from a register value
 */
static inline uint64_t extract_bits(uint64_t val, int shift, int width) {
    return (val >> shift) & ((1ULL << width) - 1);
}

/**
 * Extract a single bit from a register value
 */
static inline int extract_bit(uint64_t val, int bit) {
    return (val >> bit) & 1;
}

/**
 * Convert TCF field value to string
 */
const char* tcf_to_string(uint64_t tcf) {
    switch(tcf) {
        case 0: return "Disabled (No tag checks)";
        case 1: return "Synchronous (Tag check faults cause sync exception)";
        case 2: return "Asynchronous (Tag check faults are async)";
        case 3: return "Asymmetric (Sync reads, Async writes - FEAT_MTE3)";
        default: return "Unknown";
    }
}


/********************************/
/* print_mte_regs               */
/********************************/
void print_mte_regs() {
    long level = _NTO_IO_LEVEL_2;

    // Try to read the registers at EL1
    if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)level) == -1) {
        failed(ThreadCtl, errno);
        return;
    }

    uint64_t sctlr_el1 = 0;
    uint64_t tcr_el1 = 0;
    uint64_t gcr_el1 = 0;
    
    // SCTLR_EL1
    sctlr_el1 = read_sctlr_el1();
    int ata0 = extract_bit(sctlr_el1, SCTLR_ATA0_BIT);
    uint64_t tcf0 = extract_bits(sctlr_el1, SCTLR_TCF0_SHIFT, 2);

    printf("SCTLR_EL1.ATA0 (bit 42):  %d - %s\n", 
           ata0, 
           ata0 ? "Enabled (EL0 can access allocation tags)" : "Disabled");
    
    printf("SCTLR_EL1.TCF0 (bits 38-39): %lu - %s\n", 
           tcf0, 
           tcf_to_string(tcf0));    

    tcr_el1 = read_tcr_el1();
    int tbi0 = extract_bit(tcr_el1, TCR_TBI0_BIT);
    
    printf("TCR_EL1.TBI0 (bit 37):    %d - %s\n", 
           tbi0, 
           tbi0 ? "Enabled (Top Byte Ignore for TTBR0/EL0)" : "Disabled");
    
    // GCR_EL1
    gcr_el1 = read_gcr_el1();
    printf("GCR_EL1: 0x%016lx\n\n", gcr_el1);
 
}

extern char **environ;

/**
 * Create a new environment by copying current environment and adding MEMTAG=1
 * 
 * @return Newly allocated environment array (caller must free)
 */
char** create_memtag_environment(void) {
    // Count current environment variables
    int env_count = 0;
    while (environ[env_count] != NULL) {
        env_count++;
    }
    
    // Allocate new environment (original + MEMTAG=1 + NULL terminator)
    char **new_env = malloc(sizeof(char*) * (env_count + 2));
    if (!new_env) {
        perror("malloc");
        return NULL;
    }
    
    // Copy existing environment
    for (int i = 0; i < env_count; i++) {
        new_env[i] = strdup(environ[i]);
        if (!new_env[i]) {
            perror("strdup");
            // Cleanup on error
            for (int j = 0; j < i; j++) {
                free(new_env[j]);
            }
            free(new_env);
            return NULL;
        }
    }
    
    // Add MEMTAG=1
    new_env[env_count] = strdup("MEMTAG=1");
    if (!new_env[env_count]) {
        perror("strdup");
        for (int i = 0; i < env_count; i++) {
            free(new_env[i]);
        }
        free(new_env);
        return NULL;
    }
    
    // NULL terminator
    new_env[env_count + 1] = NULL;
    
    return new_env;
}

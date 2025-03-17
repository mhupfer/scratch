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

/**
 * @brief Coresgiht process tracer
 * @author Michael Hupfer
 * @email mhupfer@qnx.com
 * creation date 2025/03/317
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <setjmp.h>
#include <sys/mman.h>

// build: ntoaarch64-gcc cstrace.c -o cstrace -Wall -g
// deploy: vmscp.sh -f jarvis -d ~/export/aarch64le ~/scratch/cstrace

/********************************/
/* */
/********************************/
typedef enum coresight_type {
    ETM,
    FUNNEL,
    ETF
} coresight_type_t;


/********************************/
/* coresight_dev_t              */
/********************************/
typedef struct coresight_dev {
    coresight_type_t    cs_type;
    off_t               phys;
    void*               v;
    size_t              size;
} coresight_dev_t;


/********************************/
/* etm_t                        */
/********************************/
typedef struct etm {
    coresight_dev_t dev;
    unsigned        cpu;
} etm_t;


/********************************/
/* etm_t                        */
/********************************/
typedef struct funnel {
    coresight_dev_t dev;
} funnel_t;

/********************************/
/* etf_t                        */
/********************************/
typedef struct etf {
    coresight_dev_t dev;
} etf_t;


/********************************/
/* range_t                      */
/********************************/
typedef struct range {
    off_t   start;
    off_t   end;
} range_t;

#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))


etm_t imx8mpm_tracemcros[] = {
    {.dev.cs_type = ETM, .dev.phys = 0x28440000, .dev.size = 0x1000, .cpu = 0},
    {.dev.cs_type = ETM, .dev.phys = 0x28540000, .dev.size = 0x1000, .cpu = 1},
    {.dev.cs_type = ETM, .dev.phys = 0x28640000, .dev.size = 0x1000, .cpu = 2},
    {.dev.cs_type = ETM, .dev.phys = 0x28740000, .dev.size = 0x1000, .cpu = 3},
};

funnel_t imx8mpm_funnels[] = {
    {.dev.cs_type = FUNNEL, .dev.phys = 0x28c03000, .dev.size = 0x1000},
};

etf_t imx8mpm_etf[] = {
    {.dev.cs_type = ETF, .dev.phys = 0x28c04000, .dev.size = 0x1000},
};


range_t imx8mpm_ranges[] = {
    {.start = 0x28400000, .end = 0x2844ffff},
    {.start = 0x28500000, .end = 0x2854ffff},
    {.start = 0x28600000, .end = 0x2864ffff},
    {.start = 0x28700000, .end = 0x2874ffff},
    {.start = 0x28c00000, .end = 0x28c0ffff},
    {.start = 0, .end = 0},
};

/********************************/
/* cstypetostr                  */
/********************************/
char *cstypetostr(coresight_type_t t) {
    static char typenamebuf[64];
    switch (t)
    {
    case ETM:
        snprintf(typenamebuf, sizeof(typenamebuf), "ETM");
        break;
        case FUNNEL:
        snprintf(typenamebuf, sizeof(typenamebuf), "Funnel");
        break;
        case ETF:
        snprintf(typenamebuf, sizeof(typenamebuf), "ETF");
        break;
    default:
        snprintf(typenamebuf, sizeof(typenamebuf), "UNK");
        break;
    }

    return typenamebuf;
}

#define CS_DEV_SIZE 0x1000

#define AMBA_NR_IRQS	9
#define AMBA_CID	    0xb105f00d
#define CORESIGHT_CID	0xb105900d

#define CORESIGHT_CIDRn(i)	(0xFF0 + ((i) * 4))


/********************************/
/* coresight_get_cid            */
/********************************/
static inline uint32_t coresight_get_cid(void *base)
{
	uint32_t i, cid = 0;

	for (i = 0; i < 4; i++)
		cid |= *(uint32_t*)(base + CORESIGHT_CIDRn(i)) << (i * 8);

	return cid;
}

/********************************/
/* is_coresight_device          */
/********************************/
static inline bool is_coresight_device(void *base)
{
	uint32_t cid = coresight_get_cid(base);

	return cid == CORESIGHT_CID;
}


jmp_buf         env;

/********************************/
/* sighandler                   */
/********************************/
void sighandler(int sig) {
    longjmp(env, 1);
}


/********************************/
/* detect_coresight_devs        */
/********************************/
coresight_dev_t *detect_coresight_devs(range_t *ranges) {
    const unsigned  grow = 10;          // steps in which the list grows
    unsigned        listsize = grow;    // size of the list
    unsigned        devcount = 0;       // number of devs in the list
    off_t           p;
    range_t         *r;

    coresight_dev_t *devs = calloc(listsize, sizeof(*devs));

    if (devs == NULL) {
        failed(calloc, ENOMEM);
        return NULL;
    }

    signal(SIGSEGV, sighandler);
    signal(SIGBUS, sighandler);

    for (r = ranges; r->start != 0; r++) {
        for (p = r->start; p < r->end; p += CS_DEV_SIZE) {
            printf("Probing 0x%lx\n", p);
            fflush(stdout);

            void *v = mmap(NULL, CS_DEV_SIZE, PROT_READ | PROT_NOCACHE,
                           MAP_SHARED | MAP_PHYS, NOFD, p);

            if (v != MAP_FAILED) {
                if (setjmp(env) == 0U) {
                    if (is_coresight_device(v)) {
                        printf("Found cs dev 0x%lx\n", p);
                        fflush(stdout);

                        if (devcount == listsize) {
                            listsize += grow;

                            devs = realloc(devs, listsize);

                            if (devs == NULL) {
                                goto finish;;
                            }

                            devs[devcount].cs_type = 0;
                            devs[devcount].phys = p;
                            devs[devcount].size = CS_DEV_SIZE;
                            devcount++;
                        }
                    }
                };
                munmap(v, CS_DEV_SIZE);
            } else {
                failed(mmap, errno);

                if (devs) {
                    free(devs);
                    devs = NULL;
                }
                goto finish;
            }
        }
    }
finish:
    signal(SIGSEGV, NULL);
    signal(SIGBUS, NULL);
    return devs;
}


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    coresight_dev_t *list_of_devs = detect_coresight_devs(imx8mpm_ranges);

    if (list_of_devs) {
        coresight_dev_t *dev;

        for(dev = list_of_devs; dev->phys != 0; dev++) {
            printf("%s at 0x%lx", cstypetostr(dev->cs_type), dev->phys);
        }
    }

    return EXIT_SUCCESS;
}

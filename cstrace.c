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
    UNK,
    ETMv4x,
    ETMvxx,
    FUNNEL,
    TMC_ETB,
    TMC_ETR,
    TMC_ETF,
    CTI
} coresight_type_t;


/********************************/
/* coresight_dev_t              */
/********************************/
typedef struct coresight_dev {
    coresight_type_t    cs_type;
    off_t               phys;
    void*               v;
    size_t              size;
    uint8_t             major;
    uint8_t             sub;
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
    {.dev.cs_type = ETMv4x, .dev.phys = 0x28440000, .dev.size = 0x1000, .cpu = 0},
    {.dev.cs_type = ETMv4x, .dev.phys = 0x28540000, .dev.size = 0x1000, .cpu = 1},
    {.dev.cs_type = ETMv4x, .dev.phys = 0x28640000, .dev.size = 0x1000, .cpu = 2},
    {.dev.cs_type = ETMv4x, .dev.phys = 0x28740000, .dev.size = 0x1000, .cpu = 3},
};

funnel_t imx8mpm_funnels[] = {
    {.dev.cs_type = FUNNEL, .dev.phys = 0x28c03000, .dev.size = 0x1000},
};

etf_t imx8mpm_etf[] = {
    {.dev.cs_type = TMC_ETF, .dev.phys = 0x28c04000, .dev.size = 0x1000},
};


range_t imx8mpm_ranges[] = {
    {.start = 0x28400000, .end = 0x2844ffff},
    {.start = 0x28500000, .end = 0x2854ffff},
    {.start = 0x28600000, .end = 0x2864ffff},
    {.start = 0x28700000, .end = 0x2874ffff},
    {.start = 0x28c00000, .end = 0x28cfffff},
    {.start = 0, .end = 0},
};


#define CS_DEV_SIZE 0x1000

#define readu32(base, offs) (*(uint32_t*)((base) + (offs)))

#define BITS_PER_LONG (sizeof(long)*8)
#define GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & \
	 (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define BMVAL(val, lsb, msb) ((val & GENMASK(msb, lsb)) >> lsb)

#define BIT(x) (1UL<<(x))

#define stringify(x...)	#x

/* drivers/hwtracing/coresight/coresight-priv.h */
#define AMBA_NR_IRQS	9
#define AMBA_CID	    0xb105f00d
#define CORESIGHT_CID	0xb105900d

#define CORESIGHT_DEVARCH	    0xfbc
#define CORESIGHT_AUTHSTATUS	0xfb8
#define CORESIGHT_DEVID		    0xfc8
#define CORESIGHT_DEVTYPE	    0xfcc
#define CORESIGHT_CIDRn(i)	(0xFF0 + ((i) * 4))

/* drivers/hwtracing/coresight/coresight-etm4x.h */
#define ETM_DEVARCH_ARCHITECT_MASK		GENMASK(31, 21)
#define ETM_DEVARCH_ARCHITECT_ARM		((0x4 << 28) | (0b0111011 << 21))
#define ETM_DEVARCH_PRESENT			BIT(20)
#define ETM_DEVARCH_REVISION_SHIFT		16
#define ETM_DEVARCH_REVISION_MASK		GENMASK(19, 16)
#define ETM_DEVARCH_REVISION(x)			\
	(((x) & ETM_DEVARCH_REVISION_MASK) >> ETM_DEVARCH_REVISION_SHIFT)
#define ETM_DEVARCH_ARCHID_MASK			GENMASK(15, 0)
#define ETM_DEVARCH_ARCHID_ARCH_VER_SHIFT	12
#define ETM_DEVARCH_ARCHID_ARCH_VER_MASK	GENMASK(15, 12)
#define ETM_DEVARCH_ARCHID_ARCH_VER(x)		\
	(((x) & ETM_DEVARCH_ARCHID_ARCH_VER_MASK) >> ETM_DEVARCH_ARCHID_ARCH_VER_SHIFT)

#define ETM_DEVARCH_MAKE_ARCHID_ARCH_VER(ver)			\
	(((ver) << ETM_DEVARCH_ARCHID_ARCH_VER_SHIFT) & ETM_DEVARCH_ARCHID_ARCH_VER_MASK)

#define ETM_DEVARCH_ARCHID_ARCH_PART(x)		((x) & 0xfffUL)

#define ETM_DEVARCH_MAKE_ARCHID(major)			\
	((ETM_DEVARCH_MAKE_ARCHID_ARCH_VER(major)) | ETM_DEVARCH_ARCHID_ARCH_PART(0xA13))

#define ETM_DEVARCH_ARCHID_ETMv4x		ETM_DEVARCH_MAKE_ARCHID(0x4)
#define ETM_DEVARCH_ARCHID_ETE			ETM_DEVARCH_MAKE_ARCHID(0x5)

#define ETM_DEVARCH_ID_MASK						\
	(ETM_DEVARCH_ARCHITECT_MASK | ETM_DEVARCH_ARCHID_MASK | ETM_DEVARCH_PRESENT)
#define ETM_DEVARCH_ETMv4x_ARCH						\
	(ETM_DEVARCH_ARCHITECT_ARM | ETM_DEVARCH_ARCHID_ETMv4x | ETM_DEVARCH_PRESENT)
#define ETM_DEVARCH_ETE_ARCH						\
	(ETM_DEVARCH_ARCHITECT_ARM | ETM_DEVARCH_ARCHID_ETE | ETM_DEVARCH_PRESENT)


/********************************/
/* cstypetostr                  */
/********************************/
char *
cstypetostr(coresight_dev_t *dev) {
    static char typenamebuf[64];
    switch (dev->cs_type) {
    case TMC_ETB:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(TMC_ETB));
        break;
    case TMC_ETF:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(TMC_ETF));
        break;
    case TMC_ETR:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(TMC_ETR));
        break;
    case FUNNEL:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(FUNNEL));
        break;
    case ETMv4x:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(ETMv4x));
        break;
    case ETMvxx:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(ETMvxx));
        break;
    case CTI:
        snprintf(typenamebuf, sizeof(typenamebuf), stringify(CTI));
        break;
    default:
        snprintf(typenamebuf, sizeof(typenamebuf), "UNK major %hhu sub %hhu", dev->major, dev->sub);
        break;
    }

    return typenamebuf;
}

/********************************/
/* coresight_get_cid            */
/********************************/
static inline uint32_t coresight_get_cid(void *base)
{
	uint32_t i, cid = 0;

	for (i = 0; i < 4; i++)
		cid |= readu32(base, CORESIGHT_CIDRn(i)) << (i * 8);

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
            // printf("Probing 0x%lx\n", p);
            // fflush(stdout);

            void *v = mmap(NULL, CS_DEV_SIZE, PROT_READ | PROT_NOCACHE,
                           MAP_SHARED | MAP_PHYS, NOFD, p);

            if (v != MAP_FAILED) {
                if (setjmp(env) == 0U) {
                    if (is_coresight_device(v)) {
                        // printf("Found cs dev 0x%lx\n", p);
                        // fflush(stdout);

                        if (devcount == listsize) {
                            devs = realloc(devs, (listsize + grow) * sizeof(*devs));

                            if (devs == NULL) {
                                goto finish;
                            }

                            memset(&devs[listsize], 0, grow * sizeof(*devs));
                            listsize += grow;
                        }

                        /* coresight dev recognition. Mostly nonsense. */
                        const uint32_t devtype = readu32(v, CORESIGHT_DEVTYPE);
                        const unsigned major = BMVAL(devtype, 0, 3);
                        const unsigned sub = BMVAL(devtype, 4, 7);

                        switch (major) {
                        case 1:
                            /* trace sink */
                            break;
                        case 2:
                            /* trace link */
                            if (sub == 1) {
                                devs[devcount].cs_type = FUNNEL;
                            } else {
                                /* if sub == 2 drain only via APB bus*/
                                const unsigned cfgtype = BMVAL(readu32(v, CORESIGHT_DEVID), 6, 7);
                                switch (cfgtype) {
                                case 0:
                                    devs[devcount].cs_type = TMC_ETB;
                                    break;
                                case 1:
                                    devs[devcount].cs_type = TMC_ETR;
                                    break;
                                case 2:
                                    devs[devcount].cs_type = TMC_ETF;
                                    break;
                                default:
                                    break;
                                }
                            }
                            break;
                        case 3:
                            /* trace source */
                            uint32_t devarch = readu32(v, CORESIGHT_DEVARCH);
                            if ((devarch & ETM_DEVARCH_ID_MASK) != ETM_DEVARCH_ETMv4x_ARCH) {
                                devs[devcount].cs_type = ETMv4x;
                            } else {
                                devs[devcount].cs_type = ETMvxx;
                            }
                            break;
                        case 4:
                            if (sub == 1) {
                                devs[devcount].cs_type = CTI;
                            }
                        }

                        devs[devcount].phys = p;
                        devs[devcount].size = CS_DEV_SIZE;
                        devs[devcount].major = (uint8_t)major;
                        devs[devcount].sub = (uint8_t)sub;
                        devcount++;
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

    // append terminating zero
    if (devcount == listsize) {
        devs = realloc(devs, (listsize + 1) * sizeof(*devs));

        if (devs == NULL) {
            goto finish;
        }

        memset(&devs[listsize], 0, sizeof(*devs));
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
            printf("%s at 0x%lx\n", cstypetostr(dev), dev->phys);
        }
    }

    return EXIT_SUCCESS;
}

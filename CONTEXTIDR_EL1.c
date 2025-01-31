#include <sys/neutrino.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

// ntoaarch64-gcc CONTEXTIDR_EL1.c -o CONTEXTIDR_EL1 -Wall 


#define __raw_aa64_sr_wr32(__reg, __val) \
		__asm__ __volatile__("msr " #__reg ",%0" :: "r"((uint32_t)(__val)))

#define aa64_sr_wr32(__reg, __val)		__raw_aa64_sr_wr32(__reg, __val)

#define __raw_aa64_sr_rd32(__reg)									\
		({															\
			uint32_t	__res;										\
			__asm__ __volatile__("mrs	%0," #__reg : "=r"(__res));	\
			__res;													\
		})

#define aa64_sr_rd32(__reg)		__raw_aa64_sr_rd32(__reg)


int main()
{
    long level = _NTO_IO_LEVEL_2;

    if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)level)!= -1) {
		int pid = getpid();
		uint32_t ctxid = aa64_sr_rd32(contextidr_el1);
        printf("My pid=%d CONTEXTIDR_EL1=0x%x (%u)\n", pid, ctxid, ctxid);
    } else {
        perror("ThreadCtl");
    }
    return 0;
}
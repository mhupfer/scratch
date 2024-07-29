#include <sys/neutrino.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

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

#define	AARCH64_CPACR_ENABLE	(1u << 20)

static void inline
cpu_fpu_enable(void) {
	unsigned const cpacr = aa64_sr_rd32(cpacr_el1);
	if ((cpacr & AARCH64_CPACR_ENABLE) == 0) {
		aa64_sr_wr32(cpacr_el1, cpacr | AARCH64_CPACR_ENABLE);
		//isb();
	}
}

int main()
{
    long level = _NTO_IO_LEVEL_1;

    if (ThreadCtl( _NTO_TCTL_IO_LEVEL, (void*)level)!= -1) {
        printf("before CPACR_EL1=0x%x\n", aa64_sr_rd32(cpacr_el1));
        cpu_fpu_enable();
        printf("after CPACR_EL1=0x%x\n", aa64_sr_rd32(cpacr_el1));
    } else {
        perror("ThreadCtl");
    }
    return 0;
}
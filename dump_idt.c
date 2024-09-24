#define _GNU_SOURCE 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

//build: 
struct segment_info {
	uint16_t	limit;
	uint32_t	base;
} __attribute__ ((packed));

struct x86_gate_descriptor_entry {
	_Uint16t	offset_lo;
	_Uint16t	selector;
	_Uint8t		intel_reserved;			/* always zero */
	_Uint8t		flags;
	_Uint16t	offset_hi;
} __attribute__ ((packed));


static __inline void
sidt(void *__idt)
{
	__asm __volatile("sidt (%0)" :: "r"(__idt): "memory");
}


void print_gate(unsigned idx, struct x86_gate_descriptor_entry	*desc) {
	printf("idt[%u] sel 0x%x offs 0x%x flags=0x%x res=0x%x\n", idx, (unsigned)desc[idx].selector, 
		((unsigned)desc[idx].offset_hi << 16) | (unsigned)desc[idx].offset_lo, 
		(unsigned)desc[idx].flags, (unsigned)desc[idx].intel_reserved);
}

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

int main(int argc, char *argv[]) {
    unsigned idx  = 0;

    if (argc > 1)
        idx = atoi(argv[1]);

	struct segment_info	idt = {};

	sidt(&idt);
	const unsigned nintrs = (idt.limit+1) / sizeof(struct x86_gate_descriptor_entry);

    if (idx + 1 > nintrs) {
        printf("Max idx %u\n", nintrs - 1);
    } else {
        printf("base=0x%x limit=%d\n", idt.base, idt.limit);
		void *v = mmap(NULL, idt.limit, PROT_READ, MAP_SHARED|MAP_PHYS, NOFD, idt.base);

		if (v != MAP_FAILED) {
	    	struct x86_gate_descriptor_entry 	*desc = (struct x86_gate_descriptor_entry*)v;
        	print_gate(idx, desc);
		} else {
			failed(mmap, errno);
		}
    }

    return EXIT_SUCCESS;
}
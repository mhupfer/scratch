// #/********************************************************************#
// #                                                                     #
// #                       ██████╗ ███╗   ██╗██╗  ██╗                    #
// #                      ██╔═══██╗████╗  ██║╚██╗██╔╝                    #
// #                      ██║   ██║██╔██╗ ██║ ╚███╔╝                     #
// #                      ██║▄▄ ██║██║╚██╗██║ ██╔██╗                     #
// #                      ╚██████╔╝██║ ╚████║██╔╝ ██╗                    #
// #                       ╚══▀▀═╝ ╚═╝  ╚═══╝╚═╝  ╚═╝                    #
// #                                                                     #
// #/********************************************************************#

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <unistd.h>
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
// #include <sys/trace.h>
// #include <spawn.h>

// build: ntoaarch64-gcc test_unexpected_interrupt.c -o test_unexpected_interrupt -Wall -g -I ~/mainline/stage/nto/usr/include -L ~/mainline/stage/nto/aarch64le/lib/
// build: ntox86_64-gcc test_unexpected_interrupt.c -o test_unexpected_interrupt_x86_64 -Wall -g -I ~/mainline/stage/nto/usr/include -L ~/mainline/stage/nto/x86_64/lib/



#define failed(f, e)                                                           \
    fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f,        \
            strerror(e))

#define mssleep(t) usleep((t * 1000UL))


#if 0
#define PASSED "\033[92m"
#define FAILED "\033[91m"
#define ENDC "\033[0m"

#define test_failed                                                            \
    {                                                                          \
        printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);  \
        exit(1);                                                               \
    }
#endif

#include <stdint.h>
#include <sys/elf.h>


bool stopit = false;

/********************************/
/* sigint_handler               */
/********************************/
void sigint_handler(int signal) {
    stopit = true;
}


/********************************/
/* detect_arch                  */
/********************************/
int detect_arch(char *binary) {
    int fd = open(binary, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        perror("read");
        close(fd);
        return 1;
    }

    close(fd);

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not an ELF file.\n");
        return 1;
    }

    return ehdr.e_machine;
}

#define QEMU_GICD_BASE       0x08000000UL       // GIC Distributor Base für QEMU virt
#define GICD_ISPENDR(n) (*(volatile uint32_t*)(distr_base + 0x200 + ((n) * 4)))
#define GICD_ISENABLER(n)     (*(volatile uint32_t*)(distr_base + 0x100 + ((n) * 4)))

off_t PHYS_GICD_BASE = QEMU_GICD_BASE;

/********************************/
/* trigger_interrupt_aarch64    */
/********************************/
int trigger_interrupt_aarch64(uint32_t irq_id) {
    static void *distr_base = NULL;

    
    if (distr_base == NULL) {
        printf("PHYS_GICD_BASE=0x%lx\n", PHYS_GICD_BASE);
        distr_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, PHYS_GICD_BASE);
    }

    if (distr_base == MAP_FAILED) {
        failed(mmap, errno);
        return -1;
    }

    uint32_t reg_index = irq_id / 32;
    uint32_t bit_pos = irq_id % 32;

    GICD_ISENABLER(reg_index) = (1 << bit_pos);
    GICD_ISPENDR(reg_index) = (1 << bit_pos);

    return 0;
}

#define PHYS_LAPIC_BASE 0xFEE00000

/********************************/
/* trigger_interrupt_x86_64     */
/********************************/
int trigger_interrupt_x86_64(uint32_t irq_id) {
    static uint32_t *apic_base = NULL;

    if (apic_base == NULL) {
        apic_base = (uint32_t *)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, PHYS_LAPIC_BASE);
    }

    if (apic_base == MAP_FAILED) {
        failed(mmap, errno);
        return -1;
    }

    apic_base[0x310 / 4] = 0x0;        // Destination APIC ID = 0 (self)
    apic_base[0x300 / 4] = 0x00004000 | (irq_id & 0xff); // Delivery mode: Fixed, vector 0x31
    return 0;
}

void *ioapic_base;

#define IOAPIC_BASE  0xFEC00000
#define IOREGSEL     (*(volatile uint32_t *)(ioapic_base + 0x00))
#define IOWIN        (*(volatile uint32_t *)(ioapic_base + 0x10))

// Read I/O APIC register
uint32_t ioapic_read(uint8_t reg) {
    IOREGSEL = reg;
    return IOWIN;
}

// Write I/O APIC register
void ioapic_write(uint8_t reg, uint32_t value) {
    IOREGSEL = reg;
    IOWIN = value;
}

/********************************/
/* dump_ioapic_redirection_table*/
/********************************/
// void dump_ioapic_redirection_table() {
//     ioapic_base = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, IOAPIC_BASE);

//     if (ioapic_base == MAP_FAILED) {
//         failed(mmap, errno);
//         return;
//     }

//     for (int i = 0; i < 24; i++) {  // Up to 24 IRQ inputs typically
//         uint8_t reg = 0x10 + i * 2;  // Each redirection entry: 2 regs
//         uint32_t low = ioapic_read(reg);
//         uint32_t high = ioapic_read(reg + 1);

//         uint8_t vector = low & 0xFF;
//         uint8_t delivery_mode = (low >> 8) & 0x7;
//         uint8_t dest = (high >> 24) & 0xFF;

//         printf("IRQ %2d -> Vector 0x%02X, Delivery Mode: %d, Dest APIC ID: %d\n",
//                i, vector, delivery_mode, dest);
//     }
// }

// struct idt_ptr {
//     uint16_t limit;
//     uint64_t base;
// } __attribute__((packed));

// struct idt_entry {
//     uint16_t offset_low;
//     uint16_t selector;
//     uint8_t ist;
//     uint8_t type_attr;
//     uint16_t offset_mid;
//     uint32_t offset_high;
//     uint32_t zero;
// } __attribute__((packed));

// void dump_idt() {
//     struct idt_ptr idtr;
//     __asm__ volatile("sidt %0" : "=m"(idtr));

//     printf("IDT base: 0x%016lx, limit: %u bytes\n", idtr.base, idtr.limit + 1);

//     struct idt_entry* idt = (struct idt_entry*)mmap(0, idtr.limit, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_PHYS, NOFD, idtr.base);

//     if (idt == MAP_FAILED) {
//         failed(mmap, errno);
//         return;
//     }

//     int count = (idtr.limit + 1) / sizeof(struct idt_entry);

//     for (int i = 0; i < count; i++) {
//         uint64_t offset =
//             ((uint64_t)idt[i].offset_high << 32) |
//             ((uint64_t)idt[i].offset_mid << 16) |
//             idt[i].offset_low;

//         printf("Vector 0x%02X -> Handler at 0x%016lx (Selector: 0x%04X, Type: 0x%02X)\n",
//                i, offset, idt[i].selector, idt[i].type_attr);
//     }
// }


/********************************/
/* main							*/
/********************************/
int
main(int argc, char *argv[]) {
    // printf(PASSED"Test PASS"ENDC"\n");
    struct sigevent ev;


    signal(SIGINT, sigint_handler);
#define INTR_WAIT
#ifdef INTR_WAIT
    SIGEV_INTR_INIT(&ev);
#else
    int chid = ChannelCreate(0);

    if (chid == -1) {
        failed(ChannelCreate, errno);
        return 1;
    }

    int coid = ConnectAttach(0, 0, chid, 0, 0);

    if (coid == -1) {
        failed(ConnectAttach, errno);
        return -1;
    }

    SIGEV_PULSE_INIT(&ev, coid, -1, 17, 0);
    
    if (MsgRegisterEvent(&ev, _NTO_REGEVENT_ALLOW_SELF) == -1) {
        failed(MsgRegisterEvent, errno);
        return EXIT_FAILURE;
    }
#endif

    int (*trigger_interrupt)(uint32_t irq_id);

    switch (detect_arch(argv[0])) {
        case EM_X86_64:
            trigger_interrupt = trigger_interrupt_x86_64;
            // dump_ioapic_redirection_table();
            // dump_idt();
            break;
        case EM_AARCH64:
            if (argv[1]) {
                //GICD_BASE=0xff841000 for Raspi4
                if (strncmp(argv[1], "GICD_BASE=", 10) == 0) {
                    PHYS_GICD_BASE = strtoul(&argv[1][10], NULL, 0);
                }
            }
            trigger_interrupt = trigger_interrupt_aarch64;
            break;
        default:
            printf("Unknown architecture\n");
            exit(1);
    }

    int intr_id = InterruptAttachEvent(_NTO_INTR_SYNTH_UNEXPECTED, &ev, 0);

    if (intr_id == -1) {
        failed(InterruptAttachEvent, errno);
        return EXIT_FAILURE;
    }

    trigger_interrupt(42);
    trigger_interrupt(42);
    trigger_interrupt(69);

    while (!stopit) {

#ifdef INTR_WAIT
        if (InterruptWait(0, NULL) == 0) {
#else
        struct _pulse p;

        if (MsgReceivePulse(chid, &p, sizeof(p), NULL) == 0) {
#endif
            struct interrupt_query_data data;
            int res = InterruptQuery(_NTO_INTR_QUERY_UNEXPECTED_INTR, -1, 0,
                                     &data);

            if (res == EOK) {
                printf("Source %d reports %lu unexpected interrupts\n",
                       data.vector, data.deliver_count);
            } else {
                //potentially another thread consumed the 
                //unexpected interrupts
                if (errno != ESRCH) {
                    failed(MsgReceivePulse, errno);
                    return EXIT_FAILURE;
                }
            }
        } else {
#ifdef INTR_WAIT
            if (errno != EINTR)
                failed(InterruptWait, errno);
#else
            failed(MsgReceivePulse, errno);
#endif
            break;
        }
    }

    if (InterruptDetach(intr_id) == -1) {
        failed(InterruptDetach, errno);
    }

    return EXIT_SUCCESS;
}

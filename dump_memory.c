/*
    take 2 arguments
    first is a physical address, second a number of bytes
    map the physical address to a virtual address and
    dump n bytes
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
    int fd;
    void *map_base, *virt_addr;
    off_t target;
    unsigned long offset, pa_offset;
    unsigned long page_size=sysconf(_SC_PAGESIZE);
    unsigned long page_mask=page_size-1;
    int i;
    int n=0;
    int c;
    char *endptr;
    char filename[32] = "/dev/mem";

    while ((c = getopt(argc, argv, "f:n:")) != -1)
    {
        switch (c)
        {
            case 'f':
                strncpy(filename, optarg, sizeof(filename) - 1);
                break;
            case 'n':
                n = strtol(optarg, &endptr, 0);
                if (endptr == optarg)
                {
                    fprintf(stderr, "Invalid number '%s'\n", optarg);
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
                exit(1);
        }
    }

    if (optind >= argc)
    {
        fprintf(stderr, "Expected argument after options\n");
        exit(1);
    }

    target = strtoul(argv[optind], 0, 0);

    if (n == 0)
    {
        fprintf(stderr, "Expected number of bytes to dump\n");
        exit(1);
    }

    if ((fd = open(filename, O_RDWR | O_SYNC)) == -1)
    {
        perror("open");
        exit(1);
    }

    offset = target & page_mask;
    pa_offset = target & ~page_mask;

    if ((map_base = mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, pa_offset)) == (void *) -1)
    {
        perror("mmap");
        exit(1);
    }

    printf("Memory 0x%lu mapped to %p dev %s offset: 0x%lx\n", target, map_base, filename, offset);

    virt_addr = map_base + offset;

    for (i = 0; i < n; i++)
    {
        printf("0x%08x ", *((uint32_t *) virt_addr + i));
        if (i % 16 == 15)
            printf("\n");
    }
    printf("\n");

    if (munmap(map_base, n + offset) == -1)
    {
        perror("munmap");
        exit(1);
    }

    close(fd);
    return 0;
}


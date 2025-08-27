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
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
// #include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
//#include <fcntl.h>
#include <sys/memmsg.h>
#include <sys/procmsg.h>
// #include <sys/procfs.h>
//#include <sys/trace.h>
//#include <spawn.h>

//build: ntoaarch64-gcc dump_mappings.c  -o dump_mappings -Wall -g -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#if 0
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif

int introspec_get_all_ranges(pid64_t pid64, mem_map_info_t **data, int *no_of_records);
int introspec_get_all_regions(pid64_t pid64, mem_map_info_t **data, int *no_of_records);
static long introspec_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr, uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize);
static void dump_mapinfo (mem_map_info_t *mmi);
int get_process_name(pid64_t const pid, char * const buffer, size_t const bsize);

/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    pid_t pid = getpid();

    if (argc > 1) {
        pid = atoi(argv[1]);
    }

    char pname[256];

    if (get_process_name(pid, pname, 256) == -1) {
        failed(get_process_name, 0);
        return EXIT_FAILURE;
    }

    printf("Process %s pid %d\n", pname, pid);

    mem_map_info_t *regions;
    int no_of_regions;

    if (introspec_get_all_regions(pid, &regions, &no_of_regions) < 0) {
        failed(introspec_get_all_regions, 0);
        return EXIT_FAILURE;
    }

    printf("Regions (%d)\n", no_of_regions);
    printf("-------\n");

    for (int i = 0; i < no_of_regions; i++) {
        dump_mapinfo(&regions[i]);
    }

    mem_map_info_t *ranges;
    int no_of_ranges;

    if (introspec_get_all_ranges(pid, &ranges, &no_of_ranges) < 0) {
        failed(introspec_get_all_ranges, 0);
        return EXIT_FAILURE;
    }

    printf("\nRanges (%d)\n", no_of_ranges);
    printf("------\n");

    for (int i = 0; i < no_of_ranges; i++) {
        dump_mapinfo(&ranges[i]);
    }

    return EXIT_SUCCESS;
}


/********************************/
/* dump_mapinfo                 */
/********************************/
static void dump_mapinfo (mem_map_info_t *mmi) {
    printf("0x%lx + 0x%lx flags=0x%x dev=%d offset=0x%lx paddr=0x%lx ino=%ld\n", mmi->o.vaddr, mmi->o.size,
        mmi->o.flags, mmi->o.dev, mmi->o.offset, mmi->o.paddr, mmi->o.ino);
}

/********************************/
/* introspec_get_all_regions    */
/********************************/
int introspec_get_all_regions(pid64_t pid64, mem_map_info_t **data, int *no_of_records) {
    int             rc;
    mem_map_info_t  *mi;
    size_t          mi_size = 1024;

    mi = malloc(mi_size);

    while (mi) {
        rc = introspec_mapinfo(_MEM_MAP_INFO_REGION, pid64, 4096, ~0ULL, mi, mi_size);

        if (rc > 0) {
            if (rc > mi_size) {
                free(mi);
                mi_size = rc;
                mi = malloc(mi_size);
            } else {
                *data = mi;
                *no_of_records = rc / sizeof(*mi);
                break;
            }
        } else {
            failed(introspec_mapinfo, -rc);
            break;
        }

    }

    return rc;
}

/********************************/
/* introspec_get_all_regions    */
/********************************/
int introspec_get_all_ranges(pid64_t pid64, mem_map_info_t **data, int *no_of_records) {
    int             rc;
    mem_map_info_t  *mi;
    size_t          mi_size = 1024;

    mi = malloc(mi_size);

    while (mi) {
        rc = introspec_mapinfo(_MEM_MAP_INFO_RANGE, pid64, 4096, ~0ULL, mi, mi_size);

        if (rc > 0) {
            if (rc > mi_size) {
                free(mi);
                mi_size = rc;
                mi = malloc(mi_size);
            } else {
                *data = mi;
                *no_of_records = rc / sizeof(*mi);
                break;
            }
        } else {
            failed(introspec_mapinfo, -rc);
            break;
        }

    }

    return rc;
}



/********************************/
/* introspec_mapinfo			*/
/********************************/
static long introspec_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr, uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize) {
	memset(buffer, 0, sizeof(buffer->i));
	buffer->i.type = _MEM_MAP_INFO;
	buffer->i.subtype = submsg;
	buffer->i.pid = pid64;
	buffer->i.startaddr = startaddr;
	buffer->i.endaddr = endaddr;

	return MsgSend_r(MEMMGR_COID, buffer, sizeof(buffer->i), (void*)buffer, bsize);

}

/********************************/
/* get_process_name             */
/********************************/
int get_process_name(pid64_t const pid, char * const buffer, size_t const bsize) {

    struct _proc_getname i = {
        .type = _PROC_GETNAME,
        .pid = pid
    };

    return (int)MsgSend_r(PROCMGR_COID, &i, sizeof(i), (void *)buffer, bsize);
}
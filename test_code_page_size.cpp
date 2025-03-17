#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>
// #include <sys/memmsg.h>
// #include <sys/procmsg.h>
#include <sys/procfs.h>

//build: ntox86_64-g++ test_code_page_size.cpp -o test_code_page_size -Wall -g


#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

int GetCodeSegment(paddr_t & Address, int & Size);

#if 1
#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED "Test FAILED" ENDC " at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}
#endif


/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {
    paddr_t Address = {};
    int     Size = 0;

    if (GetCodeSegment(Address, Size) == 0) {
        if (Size != 0) {
            printf(PASSED "Test PASS" ENDC"\n");
            return EXIT_SUCCESS;
        }
    }

    printf(FAILED "Test FAILED" ENDC"\n");
    return EXIT_FAILURE;
}


/********************************/
/* GetCodeSegment				*/
/********************************/
int GetCodeSegment(paddr_t & Address, int & Size)
{
    struct cNameInfo
    {
        procfs_debuginfo mInfo;
        char             mpBuff[_POSIX_PATH_MAX];
    };

    procfs_info        Info;
    procfs_status      Status;
    procfs_mapinfo    *pMapInfo = NULL,     *pMap = NULL, *pFound = NULL; 
    cNameInfo          NameInfo;

    int  Fd = 0;
    char pBuffer[50];
    int  NumMaps;
    int  NumMapsAlloced = 0;
    int  i;

    // Name of the file containing process information
    sprintf(pBuffer, "/proc/%d/as", getpid());

    if ((Fd = open(pBuffer, O_RDONLY)) == -1)
    {
        return -1;
    }
       
    // Get Process Info 
    if (devctl(Fd, DCMD_PROC_INFO, &Info, sizeof Info, 0) != EOK)
    {
        close(Fd);
        return -1;
    }

    // Make sure the process is not a ZOMBIE
    if ((Info.flags & _NTO_PF_ZOMBIE)) 
    {
    	close(Fd);
        return -1;
    }

    // Get Thread Info
    Status.tid = 1;
    if (devctl(Fd, DCMD_PROC_TIDSTATUS, &Status, sizeof Status, 0) != EOK)
    {
    	close(Fd);
        // Expect this to fail when we have looked at all the threads
        // This should not happen since we are looking at only the first thread
        return -1;
    }
    else
    {
        if (Status.tid < 1)    // Make sure there are some threads
        {
        	close(Fd);
            return -1;
        }
    }

    // Get Mapping info (how many are there)
    for (NumMaps = NumMapsAlloced + 1; NumMaps > NumMapsAlloced;) 
    {
        // Allocate space for 10 more than we think there will be
        if (!(pMap = (procfs_mapinfo *) realloc(pMapInfo, (NumMaps + 10) * sizeof *pMapInfo))) 
        {
            free(pMapInfo);
            close(Fd);
            return -1;
        }
        pMapInfo = pMap;
        NumMapsAlloced = NumMaps + 10;

        // Get Mapping Information, if it doesn't fit, we try again. 
        if (devctl(Fd, DCMD_PROC_PAGEDATA, pMapInfo, sizeof *pMapInfo * NumMapsAlloced, &NumMaps) != EOK) 
        {
            free(pMapInfo);
            close(Fd);
            return -1;
        }
    }

    // We have all the mapping info, look for the process one.
    for (pMap = pMapInfo, i = NumMaps; i--; pMap++) 
    {
        if (pMap->ino == 0)    // Means don't look at this
            continue;

        // Get Name of Map Entry
        NameInfo.mInfo.vaddr = pMap->vaddr;
        if (devctl(Fd, DCMD_PROC_MAPDEBUG, &NameInfo, sizeof NameInfo, 0) != EOK)
            continue;

        printf("pMap->vaddr 0x%lx NameInfo.mInfo.vaddr 0x%lx flags 0x%x base 0x%lx\n",
               pMap->vaddr, NameInfo.mInfo.vaddr, pMap->flags,
               Info.base_address);
        // Is the entry valid 
        if ((NameInfo.mInfo.vaddr == pMap->vaddr) && 
            (pMap->flags & MAP_ELF) && 
            (Info.base_address == pMap->vaddr)) 
        {
            pFound = pMap;
            break;
        }
    }
    
    Address = pMap->vaddr;
    Size    = pMap->size;

    free(pMapInfo);
    close(Fd);

    if (!pFound)
    {
        return -1;
    }

    return 0;
	
}

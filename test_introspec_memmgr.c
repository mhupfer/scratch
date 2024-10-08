#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
// #include <sys/neutrino.h>
// #include <pthread.h>
// #include <sys/resource.h>
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
#include <fcntl.h>

#include <sys/procfs.h>
#include <sys/procmsg.h>
#include <sys/memmsg.h>
#include <kernel/macros.h>

//build: qcc test_introspec_memmgr.c -o test_introspec_memmgr -Wall -g -I ~/mainline/stage/nto/usr/include/

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))


static long introspec_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr, uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize);

static long introspec_mapdebuginfo(pid64_t pid64, uintptr_t vaddr, mem_map_info_t *msg, size_t bsize);
static long introspec_read_mem(pid64_t pid64, uintptr_t offset, void *buffer, size_t bsize);

// void dump_memory(FILE *f, char *ptr, size_t bsize);
pid64_t getpid64(pid_t pid);
int procfs_get_mappings_of_type(int fd, int type, procfs_mapinfo **data, int *no_of_records);
int introspec_get_all_regions(pid64_t pid64, mem_map_info_t **data, int *no_of_records);
int introspec_get_pagedata_for_region(pid64_t pid64, mem_map_info_t *region, mem_map_info_t **data, int *no_of_records);
int introspec_get_ptentries_for_region(pid64_t pid64, mem_map_info_t *region, mem_map_info_t **data, int *no_of_records);
int introspec_get_paddr(pid64_t pid64, uintptr_t vaddr, uintptr_t *paddr);
int continue_proc(int fd);

#define test_failed {\
    printf("Test FAILED at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}

#define READMEM_BUF_SIZE    1024
char some_random_data[READMEM_BUF_SIZE];


/********************************/
/* main							*/
/********************************/
int main(int argc, char* argv[]) {
    pid_t           pid = -1;
    pid64_t         mypid64;
    // mem_map_info_t  kmsg;
    long            rc = 0;
    char            path_buf[PATH_MAX];
    // uintptr_t       vaddr = 0x1000;
    // int             vaddr_set = 0;
    pid64_t         pid64;

    if (argc > 1) {
        pid = atoi(argv[1]);
    } else {
        pid = getpid();
    }
    
    mypid64 = getpid64(getpid());
    
    /* _MEM_MAP_INFO_OBJECT_PATH aka MAP_DEBUG */
    int fd = open("/data/var/etc/passwd", O_RDONLY);

    if (fd != -1) {
        void *v = mmap(NULL, 0x400, PROT_READ, MAP_SHARED, fd, 0);

        if (v == MAP_FAILED) {
            failed(mmap, errno);
        } else {
            rc = introspec_mapdebuginfo(mypid64, (uintptr_t)v, (mem_map_info_t *)path_buf, PATH_MAX);

            if (strcmp((char*)((mem_map_info_t*)(path_buf))->o_dbg.path, "/data/var/etc/passwd") != 0) {
                test_failed;
            }

            munmap(v, 0x400);
        }

        close(fd);
    } else {
        failed(open, errno);
    }

    /* Read mem test */ 
    {
        char            buffer[READMEM_BUF_SIZE];
        debug_thread_t	status;

        memset(some_random_data, 0x42, sizeof(some_random_data));
        int fork_res = fork();

        switch (fork_res)
        {
        case -1:
            failed(fork, errno);
            exit(1);
            break;
        case 0:
            /* 
                child: just sleep a while  
                will be stopped by parent
                then the parent reads the mem and resumes 
                execution of the child
                readmem works for stopeed processes only,
                and we can't stop ourselves
            */
            usleep(100 *1000);
            exit(0);
            break;
        default:
            break;
        }

        /* stop child */
        sprintf(path_buf, "/proc/%d/as", fork_res);
        fd = open(path_buf, O_RDWR);

        if (fd == -1) {
            failed(open, errno);
            exit(1);
        }

        rc = devctl(fd, DCMD_PROC_STOP, &status, sizeof status, 0);

    	if (rc != 0) {
            failed(devctl(PROC_STOP), rc);
    		exit(1);
    	}

        /* read memory */
        memset(buffer, 0, sizeof(buffer));
        pid64 = getpid64(fork_res);

        rc = introspec_read_mem(pid64, (uintptr_t)some_random_data, buffer, sizeof(buffer));

        if (rc > 0) {
            if (memcmp(some_random_data, buffer, sizeof(buffer)) != 0) {
                test_failed;
            }
        } else {
            failed(introspec_read_mem, -rc);
            exit(1);
        }

        /* resume child */
        rc = continue_proc(fd);
        if (rc != 0) {
            failed(continue_proc, rc);
            exit(1);
        }

        /* cleanup - close before waitpid! */
        close(fd);
        waitpid(fork_res, NULL, 0);
    }

    /* mapinfo test, comparison against procfs */
    procfs_mapinfo  *mapinfos = NULL;
    int             no_of_mappings;
    mem_map_info_t  *regions = NULL;
    int             no_of_mappings2;
    mem_map_info_t  *ranges = NULL;
    int             no_of_ranges;

    printf("Checking pid %d\n", pid);
    pid64 = getpid64(pid);


    sprintf(path_buf, "/proc/%d/as", pid);
    fd = open(path_buf, O_RDONLY);

    if (fd == -1) {
        failed(open, errno);
        exit(1);
    }

    /* virtual memory regions */
    rc = procfs_get_mappings_of_type(fd, DCMD_PROC_MAPINFO, &mapinfos, &no_of_mappings);

    if (rc == EOK) {
        rc = introspec_get_all_regions(pid64, &regions, &no_of_mappings2);

        if (rc > 0) {
            if (no_of_mappings2 != no_of_mappings || memcmp(regions, regions, no_of_mappings * sizeof *mapinfos) != 0) {
                printf("procfs mapinfo\n");
                printf("--------------\n");
                for (unsigned u = 0; u < no_of_mappings; u++) {
                    printf("v=%lx size = %lx offset = %lx paddr = %lx\n", mapinfos[u].vaddr, mapinfos[u].size, mapinfos[u].offset, mapinfos[u].paddr);
                }

                printf("introspec mapinfo\n");
                printf("-----------------\n");
                for (unsigned u = 0; u < no_of_mappings2; u++) {
                    printf("v=%lx size = %lx offset = %lx paddr = %lx\n", regions[u].o.vaddr, 
                        regions[u].o.size, regions[u].o.offset, regions[u].o.paddr);
                }

                test_failed;
            }

        } else {
            failed(introspec_get_all_regions, rc);
            exit(1);
        }

    } else {
        failed(procfs_get_mappings_of_type, rc);
        exit(1);
    }

    /* get page data for all regions */
    unsigned    range_offset = 0;
    rc = procfs_get_mappings_of_type(fd, DCMD_PROC_PAGEDATA, &mapinfos, &no_of_mappings);

    if (rc == EOK) {
        for (unsigned u = 0; u < no_of_mappings2; u++) {
            rc = introspec_get_pagedata_for_region(pid64, &regions[u], &ranges, &no_of_ranges);

            if (rc > 0) {
                if (range_offset + no_of_ranges <= no_of_mappings) {
                    if (memcmp(&mapinfos[range_offset], ranges, no_of_ranges) == 0) {
                        range_offset += no_of_ranges;

                        /* check pagetable entries for the ranges */
                        procfs_mapinfo  pt;
                        uintptr_t       paddr;

                        for (unsigned k = 0; k < no_of_ranges; k++) {
                            if (ranges[k].o.paddr == -1)
                                continue;

                            pt.vaddr = ranges[k].o.vaddr;
                            pt.size = 0x1000;

                            rc = devctl(fd, DCMD_PROC_PTINFO, &pt, sizeof(pt), NULL);

                            if (rc == EOK) {
                                rc = introspec_get_paddr(pid64, ranges[k].o.vaddr, &paddr);

                                if (rc == 1) {
                                    if (paddr != pt.paddr) {
                                        test_failed;
                                    }
                                } else {
                                    failed(introspec_get_paddr, -rc);
                                    exit(1);
                                }
                            } else {
                                failed(devctl, rc);
                                exit(1);
                            }
                        }

                    } else {
                        test_failed;
                    }
                }
            } else {
                failed(introspec_get_pagedata_for_region, rc);
                exit(1);
            }
        }

        if (range_offset != no_of_mappings) {
            test_failed;
        }
    } else {
        failed(procfs_get_mappings_of_type, rc);
        exit(1);
    }

    printf("Test PASSED\n");
    return 0;
}


/********************************/
/* _get_mappings_of_type        */
/********************************/
int procfs_get_mappings_of_type(int fd, int type, procfs_mapinfo **data, int *no_of_records) {
    int             res, no_of_mappings, cur_no_of_mappings;
    procfs_mapinfo  *mapinfos = NULL;

    do {
        res = devctl(fd, type, NULL, 0, &no_of_mappings);

	    if (res == EOK) {
	    	mapinfos = realloc(mapinfos, no_of_mappings * sizeof(*mapinfos));

            if (mapinfos) {
                res = devctl(fd, type, mapinfos, no_of_mappings * sizeof(*mapinfos), &cur_no_of_mappings);

                if (res == EOK) {
                    if (no_of_mappings >= cur_no_of_mappings) ranges[k].o.vaddr == -1
                        break;
                    }
                } else {
                    failed(devctl, res);
                }
            } else {
                res = ENOMEM;
                failed(realloc, res);
            }
	    } else {
	    	failed(devctl, res);
        }
    } while (res == EOK);

    if (res == EOK) {
        *data = mapinfos;
        *no_of_records = no_of_mappings;
    }

    return res;
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
        }

    }

    return rc;
}


/*************************************/
/* introspec_get_pagedata_for_region */
/*************************************/
int introspec_get_pagedata_for_region(pid64_t pid64, mem_map_info_t *region, mem_map_info_t **data, int *no_of_records) {
    int             rc;
    mem_map_info_t  *mi;
    size_t          mi_size = 1024;

    mi = malloc(mi_size);

    while (mi) {
        rc = introspec_mapinfo(_MEM_MAP_INFO_RANGE, pid64, region->o.vaddr, region->o.vaddr + region->o.size - 1, mi, mi_size);

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
        }

    }

    return rc;
}


/*************************************/
/* introspec_get_ptentries_for_region */
/*************************************/
int introspec_get_paddr(pid64_t pid64, uintptr_t vaddr, uintptr_t *paddr) {
    int             rc;
    mem_map_info_t  mi;

    rc = introspec_mapinfo(_MEM_MAP_INFO_PAGETABLE, pid64, vaddr, vaddr, &mi, sizeof(mi));
    *paddr = mi.o.paddr;

    return rc / sizeof(mi);
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
/* introspec_mapdebuginfo		*/
/********************************/
static long introspec_mapdebuginfo(pid64_t pid64, uintptr_t vaddr, mem_map_info_t *msg, size_t bsize) {
	if (bsize < sizeof(msg->i)) {
		return EBADMSG;
	}

	memset(msg, 0, sizeof(msg->i));
	msg->i.type = _MEM_MAP_INFO;
	msg->i.subtype = _MEM_MAP_INFO_OBJECT_PATH;
	msg->i.pid = pid64;
	msg->i.startaddr = vaddr;
	
	return MsgSend_r(MEMMGR_COID, &msg->i, sizeof(msg->i), &msg->o_dbg, bsize);

}


/********************************/
/* introspec_read_mem			*/
/********************************/
static long introspec_read_mem(pid64_t pid64, uintptr_t offset, void *buffer, size_t bsize) {
	mem_read_mem_t *msg = buffer;

	if (bsize < sizeof(msg->i)) {
		return EBADMSG;
	}

	memset(msg, 0, sizeof(msg->i));
	msg->i.type = _MEM_READ_MEM;
	msg->i.subtype = 0;
	msg->i.pid = pid64;
	msg->i.startaddr = offset;

	return MsgSend_r(MEMMGR_COID, &msg->i, sizeof(msg->i), buffer, bsize);
}


/********************************/
/* dump_memory                  */
/********************************/
// void dump_memory(FILE *f, char *ptr, size_t bsize) {
//     unsigned    v;

//     for (v = 0; v < bsize; v++) {
//         fprintf(f, "%02hhx ", ptr[v]);

//         if ((v % 16) == 15) {
//             fprintf(f, "\n");
//         }
//     }

//     /* print final line feed if the last line was not comeplete */
//     if ((v % 16) < 15) {
//         fprintf(f, "\n");
//     }
// }


/********************************/
/* getpid64                     */
/********************************/
pid64_t getpid64(pid_t pid) {
    proc_getpid64_t pid64msg;

    pid64msg.i.type = _PROC_GETPID64;
    pid64msg.i.pid = pid;

    int rc = -MsgSend_r(PROCMGR_COID, &pid64msg.i, sizeof(pid64msg.i), &pid64msg.o, sizeof(pid64msg.o));

    if (rc == EOK) {
        return pid64msg.o;
    } else {
        failed(MsgSend_r, rc);
        return -1;
    }

}


/********************************/
/* continue_proc                */
/********************************/
int continue_proc(int fd) {
    procfs_run		run;

    memset( &run, 0, sizeof(run) );
	// run.flags |= _DEBUG_RUN_ARM;
	run.flags |= _DEBUG_RUN_CLRFLT | _DEBUG_RUN_CLRSIG;

    int res = devctl(fd, DCMD_PROC_RUN, &run, sizeof(run), 0);
	if (res != 0) {
		printf("devctl(DCMD_PROC_RUN) failed: %s\n", strerror(res));
    }
    return res;
}



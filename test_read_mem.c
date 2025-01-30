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
#include <sys/mman.h>
// #include <time.h>
// #include <assert.h>
// #include <fcntl.h>
#include <sys/memmsg.h>

//build: qcc test_read_mem.c -o test_read_mem -Wall -g

#define failed(f, e) fprintf(stderr, "%s:%d: %s() failed: %s\n", __func__, __LINE__, #f, strerror(e))

#define usleep_ms(t) usleep((t*1000UL))

#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    _test_failed = 1;\
}

#define PAGE_SIZE 0x1000
#define ADDRESS_BOUNDARY 0x0000007fffffffffUL
#define KERNEL_ADDRESS (ADDRESS_BOUNDARY + 0x800001UL)
#define HIGH_USERPACE_ADDR (ADDRESS_BOUNDARY - 0x2fff)

char buffer[PAGE_SIZE];

int read_mem_memmgr(uintptr_t addr, char*buffer, unsigned blen);
int read_mem_procfs(uintptr_t addr, char* buffer, unsigned blen);
int write_mem_procfs(uintptr_t addr, char* buffer, unsigned blen);
static long get_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr, uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize);

/********************************/
/* main							*/
/********************************/
int main(int argn, char* argv[]) {

    int _test_failed = 0;

    // reading a kernel address should fail bcs. fseek fails
    if (read_mem_procfs(KERNEL_ADDRESS, buffer, PAGE_SIZE) != -1 || errno != EINVAL) {
        test_failed;
    }

    if (read_mem_memmgr(KERNEL_ADDRESS, buffer, PAGE_SIZE) != -ESRVRFAULT) {
        test_failed;
    }

    /* check if vm_mapinfo returns 0 on HIGH_USERPACE_ADDR */
    int res = get_mapinfo(_MEM_MAP_INFO_REGION, 0, HIGH_USERPACE_ADDR, HIGH_USERPACE_ADDR + 0x1000, (mem_map_info_t*)buffer, sizeof(buffer));

    if (res < 0) {
        failed(get_mapinfo, res);
        return EXIT_FAILURE;
    }

    if (res > 0) {
        printf(FAILED"Adress 0x%lx is unsuitable for the test"ENDC"\n", HIGH_USERPACE_ADDR);
        return EXIT_FAILURE;
    }

    //reading the high userpace addres should fail
    if (read_mem_procfs(HIGH_USERPACE_ADDR, buffer, PAGE_SIZE) != -1 || errno != ESRVRFAULT) {
        test_failed;
    }

    if (read_mem_memmgr(HIGH_USERPACE_ADDR, buffer, PAGE_SIZE) != -ESRVRFAULT) {
        test_failed;
    }

    //writing to address "page" should fail too
    if (write_mem_procfs(HIGH_USERPACE_ADDR, buffer, PAGE_SIZE) != -1 || errno != ESRVRFAULT) {
        test_failed;
    }

    /* check if readin memory succeeds at all */
    uintptr_t *v = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, NOFD, 0);

    if (v == MAP_FAILED) {
        failed(mmap, errno);
        return EXIT_FAILURE;
    }

    memset(v, 0x42, PAGE_SIZE);
    memset(buffer, 0, PAGE_SIZE);

    uintptr_t page = (uintptr_t)v;

    //reading mem from procfs succeeds
    if (read_mem_procfs(page, buffer, PAGE_SIZE) != PAGE_SIZE) {
        return EXIT_FAILURE;
    }

    if (memcmp(buffer, v, PAGE_SIZE)) {
        test_failed;
    }

    //reading mem using memmgr interface succeeds
    memset(buffer, 0, PAGE_SIZE);
    if (read_mem_memmgr(page, buffer, PAGE_SIZE) != PAGE_SIZE) {
        failed(mmap, errno);
        return EXIT_FAILURE;
    }

    if (memcmp(buffer, v, PAGE_SIZE)) {
        test_failed;
    }

    /* read 0 bytes */
    mem_read_mem_t msg;

	memset(&msg, 0, sizeof(msg.i));
	msg.i.type = _MEM_READ_MEM;
	msg.i.subtype = 0;
	msg.i.pid = 0;
	msg.i.startaddr = 0;


    if (MsgSend_r(MEMMGR_COID, &msg.i, sizeof(msg.i), buffer, 0) != -ESRVRFAULT) {
        failed(mmap, errno);
        return EXIT_FAILURE;
    }

    if (!_test_failed)
        printf(PASSED"Test PASS"ENDC"\n");

    return EXIT_SUCCESS;
}


/********************************/
/* read_mem_memmgr              */
/********************************/
int read_mem_memmgr(uintptr_t addr, char* buffer, unsigned blen) {
	mem_read_mem_t *msg = (mem_read_mem_t*)buffer;

	if (blen < sizeof(msg->i)) {
		return EBADMSG;
	}

	memset(msg, 0, sizeof(msg->i));
	msg->i.type = _MEM_READ_MEM;
	msg->i.subtype = 0;
	msg->i.pid = 0;
	msg->i.startaddr = addr;

	return MsgSend_r(MEMMGR_COID, &msg->i, sizeof(msg->i), buffer, blen);

}


/********************************/
/* read_mem_procfs              */
/********************************/
int read_mem_procfs(uintptr_t addr, char* buffer, unsigned blen) {
    FILE *f = fopen("/proc/self/as", "r");

    if (f == NULL) {
        failed(fopen, errno);
        return -1;
    }

    if (fseek(f, (off64_t)addr, SEEK_SET) != EOK) {
        failed(fseek, errno);
        return -1;
    }

    if (fread(buffer, blen, 1, f) != 1) {
        failed(fread, errno);
        return -1;
    }

    fclose(f);
    return blen;
}


/********************************/
/* read_mem_procfs              */
/********************************/
int write_mem_procfs(uintptr_t addr, char* buffer, unsigned blen) {
    FILE *f = fopen("/proc/self/as", "w");

    if (f == NULL) {
        failed(fopen, errno);
        return -1;
    }

    if (fseek(f, (off64_t)addr, SEEK_SET) != EOK) {
        failed(fseek, errno);
        return -1;
    }

    if (fwrite(buffer, blen, 1, f) != 1) {
        failed(fwrite, errno);
        return -1;
    }

    fclose(f);
    return blen;
}


/********************************/
/* get_mapinfo      			*/
/********************************/
static long get_mapinfo(uint16_t submsg, pid64_t pid64, uintptr_t startaddr, uintptr_t endaddr, mem_map_info_t *buffer, size_t bsize) {
	memset(buffer, 0, sizeof(buffer->i));
	buffer->i.type = _MEM_MAP_INFO;
	buffer->i.subtype = submsg;
	buffer->i.pid = pid64;
	buffer->i.startaddr = startaddr;
	buffer->i.endaddr = endaddr;

	return MsgSend_r(MEMMGR_COID, buffer, sizeof(buffer->i), (void*)buffer, bsize);

}


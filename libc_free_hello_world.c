#include <stdlib.h>
#include <unistd.h>
#include <sys/iomsg.h>

#if __SIZEOF_SIZE_T__ > 4
	#define SIZET_VAL_GET_HI32(t) (uint32_t)((t) >> 32)
#else
	#define SIZET_VAL_GET_HI32(t) 0
#endif

// build: ntoaarch64-gcc -fpie -pie -nostartfiles -nostdlib -Wall ~/scratch/libc_free_hello_world.c -o ~/scratch/libc_free_hello_world -fno-stack-protector -g


/********************************/
/* static_MsgSendv              */
/********************************/
static long static_MsgSendv(int coid, const struct iovec *siov, _Sizet sparts, const struct iovec *riov, _Sizet rparts)
{
    asm(
    "mov	w8, #0xb    \n"
    "svc	#0x51    \n"
    );

    return 0;
}

/********************************/
/* static_write                 */
/********************************/
static ssize_t static_write(int fd, const void *buff, size_t nbytes) {
	io_write_t				msg = { 0 };
	iov_t					iov[2];

	msg.i.xtype = _IO_XTYPE_NONE;
	msg.i.nbytes = (uint32_t)nbytes;
	msg.i64.nbytes_hi = SIZET_VAL_GET_HI32(nbytes);
	if(msg.i64.nbytes_hi == 0) {
		msg.i.type = _IO_WRITE;
	} else {
		msg.i.type = _IO_WRITE64;
	}
	SETIOV(iov + 0, &msg.i, sizeof msg.i);
	SETIOV_CONST(iov + 1, buff, nbytes);
	long rc = static_MsgSendv(fd, iov, 2, 0, 0);
	if((rc < -1) || (rc > (long)nbytes)) {
		rc = -1;
	}
	return rc;
}

int main(void) {
    const char msg[] = "Hello libc-free world!\n";
    static_write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    return 0;
}

int ThreadDestroy(int tid, int priority, void *status) {
    asm(
    "mov	w8, #0x2f    \n"
    "svc	#0x51    \n"
    );

    return 0;
}

void _start() {
    main();
    for (;;) {
        (void) ThreadDestroy(-1, -1, (void *)(intptr_t) 0);
    }
}
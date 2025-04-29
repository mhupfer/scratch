
/**
 * @file  storage_test.c
 *
 * Stress-test "snapshots" on a device.
 */

/**
 * @brief It does something similar to the QNX6-FS. It writes a series of data blocks 
 * (filled with calculable values), whereby the writes may be cached. Then it writes a 
 * “superblock” that describes the data blocks and is written synchronously, like the 
 * qnx6fs. Like the FS, it uses two superblocks (1 at the front, 1 at the back), 
 * between which it switches back and forth. In addition, each “superblock” also 
 * contains a signature (so that the program can recognize if a test has previously 
 * 0been run on a device) and an ascending sequence number.
 * At the beginning, the program checks whether it finds two valid superblocks. 
 * If so, it takes the newer one and checks whether all blocks of the “snapshot” 
 * contain correct patterns. If not: error. Otherwise, it re-initializes the 
 * superblocks on the device and starts the stress test. After each “snapshot” 
 * t checks again whether the blocks of the penultimate one (which will now be 
 * overwritten) are still valid. If not: error. So: System start, let the program 
 * run for a while, switch off the system, rinse & repeat
 * 
 * Translated with DeepL.com (free version)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>


/****************************************************************************
	CONSTANTS AND MACROS
 ****************************************************************************/

/**
 * @brief  Superblock signature.
 */
#define SBLK_SIG  ((uint32_t) UINT32_C(0x267051f3))

/**
 * @brief  Superblock size [bytes].
 */
#define SBLKSIZE  (512U)

/**
 * @brief  Data block size [bytes].
 */
#define BLKSIZE  (4096U)

/**
 * @brief  Number of 32-bits words per data block.
 */
#define WORDS_PER_BLK  (BLKSIZE / sizeof(uint32_t))

/**
 * @brief  Number of 32-bit block numbers in the superblock.
 */
#define SBLK_BLKS  ((SBLKSIZE - sizeof(sblk_hdr_t)) / sizeof(uint32_t))


/**
 * @brief  Divide @n by @m, rounding up.
 */
#define RUPDIV(n, m)  (((n) + (m) - 1U) / (m))

/**
 * @brief  Round up @n to the next higher integer multiple of @m.
 */
#define RUP(n, m)  (RUPDIV((n), (m)) * (m))

/**
 * @brief  Round down @n to the next lower integer multiple of @m.
 */
#define RDN(n, m)  (((n) / (m)) * (m))


/****************************************************************************
	TYPE DEFINITIONS
 ****************************************************************************/

/**
 * @brief  Device descriptor.
 */
typedef struct {
	char const   *path;    /*!< Pathname of device     */
	int           fd;      /*!< Device file descriptor */
	/***** TODO: ADD ANY FURTHER INFO REQUIRED FOR DEVICE ACCESS *****/
} dvc_t;

/**
 * @brief  Operation modes for writing to a device.
 */
typedef enum {
	DW_ASYNC,    /*!< Asynchronous write (caching allowed) */
	DW_SYNC      /*!< Synchronous write  (flush all)       */
} dw_mode_t;

/**
 * @brief  A data block.
 */
typedef struct {
	uint32_t    word[WORDS_PER_BLK];    /*!< Data words */
} blk_t;

/**
 * @brief  Superblock header.
 */
typedef struct {
	uint32_t    sig;      /*!< Signature                    */
	uint32_t    seqno;    /*!< sequence number              */
	uint32_t    nblks;    /*!< Number of blocks in snapshot */
} sblk_hdr_t;

/**
 * @brief  Superblock.
 */
typedef struct {
	sblk_hdr_t    hdr;                /*!< Header                                     */
	uint32_t      blks[SBLK_BLKS];    /*!< Block numbers (first @hdr.nblks are valid) */
} sblk_t;

/**
 * @brief  A "file system".
 */
typedef struct {
	dvc_t      *dvc;            /*!< Host device                    */
	uint64_t    sblk_pos[2];    /*!< Superblock positions           */
	unsigned    blkno_min;      /*!< Minimum block number           */
	unsigned    blkno_max;      /*!< Maximum block number           */
	uint8_t    *bmp;            /*!< Bitmap                         */
	sblk_t      sblks[2];       /*!< Superblocks                    */
	unsigned    sblk_idx;       /*!< Current superblock index (0/1) */
	sblk_t     *sblk;           /*!< Current superblock pointer     */
	blk_t       blkbuf;         /*!< Block I/O buffer               */
} fs_t;


/****************************************************************************
	GENERIC HELPER FUNCTIONS
 ****************************************************************************/

/*!
 * @brief  Print an error message and exit.
 *
 * @param [in]  ecode  EOK, or some @errno code.
 * @param [in]  fmt    printf()-like format string for error message.
 */
static void
ferr(
		int const ecode,
		char const *const fmt,
		...) {
	va_list  alist;
	fprintf(stderr, "Error: ");
	va_start(alist, fmt);
	vfprintf(stderr, fmt, alist);
	va_end(alist);
	if(ecode != EOK) {
		fprintf(stderr, " -- %s\n", strerror(ecode));
	} else {
		fprintf(stderr, "\n");
	}
	exit(EXIT_FAILURE);
}

/*!
 * @brief  Print usage information and exit.
 */
static void
usage(void) {
	fprintf(stderr,
"Usage:\n"
"storage_test <partition-device>\n"
	);
	exit(EXIT_FAILURE);
}

/*!
 * @brief  Provide a pseudo-random number from a given range.
 *
 * @param [in]  mn  Minimum return value.
 * @param [in]  mx  Maximum return value.
 *
 * @return  A pseudo-random number in the range [@mn..@mx].
 */
static unsigned
urand(unsigned const mn, unsigned const mx) {
	unsigned const  rnd = (unsigned) rand();
	return ((rnd % (mx - mn + 1U)) + mn);
}


/****************************************************************************
	DEVICE FUNCTIONS
 ****************************************************************************/

/*!
 * @brief  Open a given device and initialize its descriptor.
 *
 * @param [out]  dvc   Device descriptor to initialize.
 * @param [in]   path  Pathname of device to open.
 */
static void
dvc_open(dvc_t *const dvc, char const *const path) {
	/***** TODO: IMPLEMENT DIFFERENTLY? *****/
	*dvc = (dvc_t const) { .path=NULL, .fd=-1 };
	dvc->fd = open(path, O_RDWR);
	if(dvc->fd == -1) {
		ferr(errno, "Opening '%s' failed", path);
	}
	dvc->path = path;
}

/*!
 * @brief  Get the size of a device.
 *
 * @param [in]  dvc  Device to query.
 *
 * @return  The device's size (in bytes).
 */
static uint64_t
dvc_get_size(dvc_t const *const dvc) {
	/***** TODO: IMPLEMENT DIFFERENTLY? *****/
	struct stat  st;
	if(fstat(dvc->fd, &st) == -1) {
		ferr(errno, "stat() failed on '%s'", dvc->path);
	}
	return (uint64_t) st.st_blocksize * (uint64_t) st.st_nblocks;
}

/*!
 * @brief  Read @num bytes of data into @buf from device @dvc ,
 *         starting at byte position @off.
 *
 * @param [in]   dvc  Device to read from.
 * @param [out]  buf  Destination buffer.
 * @param [in]   num  Number of bytes to read.
 * @param [in]   off  Byte offset at which reading shall commence.
 */
static void
dvc_read(dvc_t const *const dvc, void *const buf,
         size_t const num, uint64_t const off) {
	/***** TODO: IMPLEMENT DIFFERENTLY? *****/
	ssize_t const  nrd = pread(dvc->fd, buf, num, off);
	if(nrd == -1) {
		ferr(errno, "Reading '%s' failed", dvc->path);
	}
	if((size_t) nrd != num) {
		ferr(EOK, "Short read on '%s'", dvc->path);
	}
}

/*!
 * @brief  Write @num bytes of data from @buf to device @dvc,
 *         starting at byte position @off.
 *
 * @param [in]  dvc   Device to write from.
 * @param [in]  buf   Source buffer.
 * @param [in]  num   Number of bytes to write.
 * @param [in]  off   Byte offset at which writing shall commence.
 * @param [in]  mode  Writing mode. Must be one of:
 *                      DW_ASYNC - Data may be cached
 *                      DW_SYNC - Data must be committed to storage
 *                                before the function returns.
 */
static void
dvc_write(dvc_t const *const dvc, void const *const buf,
          size_t const num, uint64_t const off,
          dw_mode_t const mode) {
	/***** TODO: IMPLEMENT DIFFERENTLY? *****/
	if(mode == DW_SYNC) {
		if(fsync(dvc->fd) == -1) {
			ferr(errno, "Synchronizing '%s' failed", dvc->path);
		}
	}
	ssize_t const  nwr = pwrite(dvc->fd, buf, num, off);
	if(nwr == -1) {
		ferr(errno, "Writing '%s' failed", dvc->path);
	}
	if((size_t) nwr != num) {
		ferr(EOK, "Short write on '%s'", dvc->path);
	}
	if(mode == DW_SYNC) {
		if(fsync(dvc->fd) == -1) {
			ferr(errno, "Synchronizing '%s' failed", dvc->path);
		}
	}
}


/****************************************************************************
	BLOCK FUNCTIONS
 ****************************************************************************/

/*!
 * @brief  Generate a block pattern.
 *
 * A block pattern is a block-sized array of 32-bit words. Each of these
 * words is assigned a unique value, which is the XOR of:
 *   - The "file-systems" current sequence number
 *   - The block's block number inside the "file system"
 *   - The word's index within the block-sized array.
 *
 * @param [out]  blk    The block to fill.
 * @param [in]   seqno  The sequence number to use.
 * @param [in]   blkno  The block number to use.
 */
static void
blk_generate_pattern(blk_t *const blk, unsigned const seqno, unsigned const blkno) {
	for(unsigned widx = 0U; widx < WORDS_PER_BLK; widx++) {
		blk->word[widx] = (uint32_t) (seqno ^ blkno ^ widx);
	}
}


/****************************************************************************
	"FILE-SYSTEM" FUNCTIONS
 ****************************************************************************/

/*!
 * @brief  Set a FS's current superblock.
 *
 * @param [inout]  fs   "File system" to operate on.
 * @param [in]     idx  The superblock index to set as the current one.
 */
static void
fs_set_cur_sblk(fs_t *const fs, unsigned const idx) {
	fs->sblk_idx = idx;
	fs->sblk = &fs->sblks[idx];
}

/*!
 * @brief  Reset a file system.
 *
 * Will set superblocks to a zero-state, reset the sequence number to zero,
 * and also zero out the bitmap.
 *
 * @param [inout]  fs  "File system" to reset.
 */
static void
fs_reset(fs_t *const fs) {
	fs->sblks[0] = (sblk_t const) { .hdr = { .sig = SBLK_SIG, .seqno = 3U, .nblks = 0U }, .blks = {0U} };
	fs->sblks[1] = (sblk_t const) { .hdr = { .sig = SBLK_SIG, .seqno = 2U, .nblks = 0U }, .blks = {0U} };
	fs_set_cur_sblk(fs, 0U);
	memset(fs->bmp, 0, RUPDIV(fs->blkno_max + 1U, 8U));
}

/*!
 * @brief  Initialize a "file system".
 *
 * @param [inout]  fs   The FS to initialize.
 * @param [in]     dvc  The device that the FS resides on.
 */
static void
fs_init(fs_t *const fs, dvc_t *const dvc) {
	fs->dvc = dvc;
	fs->sblk_pos[0] = 8192U;
	fs->sblk_pos[1] = RDN(dvc_get_size(dvc), 4096U) - 4096U;
	fs->blkno_min = RUPDIV(fs->sblk_pos[0] + sizeof(sblk_t), BLKSIZE);
	fs->blkno_max = (fs->sblk_pos[1] - BLKSIZE) / BLKSIZE;
	fs->bmp = malloc(RUPDIV(fs->blkno_max + 1U, 8U));
	if(fs->bmp == NULL) {
		ferr(errno, "Allocating bitmap failed");
	}
	fs_reset(fs);
}

/*!
 * @brief  Read a file system block.
 *
 * @param [inout]  fs     "File system" to operate on.
 * @param [in]     blkno  The block number to read.
 */
static void
fs_read_blk(fs_t *const fs, unsigned const blkno) {
	dvc_read(fs->dvc, &fs->blkbuf, BLKSIZE, (uint64_t) blkno * (uint64_t) BLKSIZE);
}

/*!
 * @brief  Write a file system block.
 *
 * @param [in]  fs     "File system" to operate on.
 * @param [in]  blkno  The block number to write.
 */
static void
fs_write_blk(fs_t const *const fs, unsigned const blkno) {
	dvc_write(fs->dvc, &fs->blkbuf, BLKSIZE, (uint64_t) blkno * (uint64_t) BLKSIZE, DW_ASYNC);
}

/*!
 * @brief  Read a file system superblock.
 *
 * @param [inout]  fs   "File system" to operate on.
 * @param [in]     idx  The superblock index to read.
 */
static void
fs_read_sblk(fs_t *const fs, unsigned const idx) {
	dvc_read(fs->dvc, &fs->sblks[idx], SBLKSIZE, fs->sblk_pos[idx]);
}

/*!
 * @brief  Write a file system superblock.
 *
 * @param [in]  fs   "File system" to operate on.
 * @param [in]  idx  The superblock index to write.
 */
static void
fs_write_sblk(fs_t const *const fs, unsigned const idx) {
	dvc_write(fs->dvc, &fs->sblks[idx], SBLKSIZE, fs->sblk_pos[idx], DW_SYNC);
}

/*!
 * @brief  Check whether a given block is used.
 *
 * @param [in]  fs     "File system" to operate on.
 * @param [in]  blkno  Block number to test.
 *
 * @return  @true  if the block is used, or
 *          @false otherwise.
 */
static bool
fs_blk_used(fs_t const *const fs, unsigned const blkno) {
	return ((fs->bmp[blkno / 8U] & (1U << (blkno % 8U))) != 0U);
}

/*!
 * @brief  Mark a given block as "used" in the bitmap.
 *
 * @param [in]  fs     "File system" to operate on.
 * @param [in]  blkno  The block to mark as used.
 */
static void
fs_blk_alloc(fs_t const *const fs, unsigned const blkno) {
	fs->bmp[blkno / 8U] |= (1U << (blkno % 8U));
}

/*!
 * @brief  Mark a given block as "unused" in the bitmap.
 *
 * @param [in]  fs     "File system" to operate on.
 * @param [in]  blkno  The block to mark as free.
 */
static void
fs_blk_free(fs_t const *const fs, unsigned const blkno) {
	fs->bmp[blkno / 8U] &= ~(1U << (blkno % 8U));
}

/*!
 * @brief  Verify all data blocks described in the "active" superblock.
 *
 * @param [in]  fs  "File system" to operate on.
 */
static void
fs_verify_blocks(fs_t *const fs) {
	static blk_t  cblkbuf;
	bool  ok = true;
	for(unsigned bidx = 0U; bidx < fs->sblk->hdr.nblks; bidx++) {
		unsigned const  blkno = fs->sblk->blks[bidx];
		fs_read_blk(fs, blkno);
		blk_generate_pattern(&cblkbuf, fs->sblk->hdr.seqno, blkno);
		if(memcmp(&fs->blkbuf, &cblkbuf, BLKSIZE) != 0) {
			fprintf(stderr, "Mismatch found: Shdr #%u, Seqno %u, Blkno %u (%u/%u)\n",
					fs->sblk_idx, fs->sblk->hdr.seqno, blkno, bidx, fs->sblk->hdr.nblks);
			ok = false;
		}
	}
	if(!ok) {
		exit(EXIT_FAILURE);
	}
}

/*!
 * @brief  Check whether the device contains a file system, and if so,
 *         verify the integrity of all its data blocks.
 *
 * @param [in]  fs  "File system" to operate on.
 */
static void
fs_verify(fs_t *const fs) {
	fs_read_sblk(fs, 0U);
	fs_read_sblk(fs, 1U);

	if((fs->sblks[0].hdr.sig != SBLK_SIG) && (fs->sblks[1].hdr.sig != SBLK_SIG)) {
		/* Assume uninitialized */
		printf("File system uninitialized\n");
	} else if(fs->sblks[0].hdr.sig != SBLK_SIG) {
		/* Assume sblk 0 is damaged */
		ferr(EOK, "Superblock #0 invalid");
	} else if(fs->sblks[1].hdr.sig != SBLK_SIG) {
		/* Assume sblk 1 is damaged */
		ferr(EOK, "Superblock #1 invalid");
	} else {
		fs_set_cur_sblk(fs, (fs->sblks[0].hdr.seqno > fs->sblks[1].hdr.seqno) ? 0U : 1U);
		fs_verify_blocks(fs);
		printf("File system initially ok at sequence number %u\n", fs->sblk->hdr.seqno);
	}

	/* Leave the FS in a fresh state */
	fs_reset(fs);
}

/*!
 * @brief  Select a random number of random free blocks
 *         that should be written in the next test round.
 *
 * @param [inout]  fs  "File system" to operate on.
 */
static void
fs_choose_blks(fs_t *const fs) {
	fs->sblk->hdr.nblks = urand(0U, SBLK_BLKS - 1U);
	for(unsigned bidx = 0U; bidx < fs->sblk->hdr.nblks; bidx++) {
		unsigned  blkno = 0U;
		do {
			blkno = urand(fs->blkno_min, fs->blkno_max);
			//TODO: This could potentially be a infinite loop,
			//      if the "file system" was too small
		} while(fs_blk_used(fs, blkno));
		fs->sblk->blks[bidx] = blkno;
	}
}

/*!
 * @brief  Write generated test patterns to all data blocks
 *         selected for this test round.
 *
 * @param [in]  fs  "File system" to operate on.
 */
static void
fs_write_blks(fs_t *const fs) {
	for(unsigned bidx = 0U; bidx < fs->sblk->hdr.nblks; bidx++) {
		unsigned const  blkno = fs->sblk->blks[bidx];
		blk_generate_pattern(&fs->blkbuf, fs->sblk->hdr.seqno, blkno);
		fs_blk_alloc(fs, blkno);
		fs_write_blk(fs, blkno);
	}
}

/*!
 * @brief  Mark all blocks indicated by the current superblock
 *         as "unused" in the bitmap.
 *
 * @param [in]  fs  "File system" to operate on.
 */
static void
fs_free_blks(fs_t *const fs) {
	for(unsigned bidx = 0U; bidx < fs->sblk->hdr.nblks; bidx++) {
		fs_blk_free(fs, fs->sblk->blks[bidx]);
	}
	fs->sblk->hdr.nblks = 0U;
}

/*!
 * @brief  Synchronize the file system.
 *
 * Finalize the current superblock and write it to disk.
 * Then switch to the other one.
 *
 * @param [inout]  fs  "File system" to operate on.
 */
static void
fs_sync(fs_t *const fs) {
	fs_write_sblk(fs, fs->sblk_idx);
	fs_set_cur_sblk(fs, fs->sblk_idx ^ 1U);
//	fs->sblk->hdr.seqno += 2U;
}

/*!
 * @brief  Execute one test cycle.
 *
 * @param [inout]  fs  "File system" to operate on.
 */
static void
fs_test_cycle(fs_t *const fs) {
	fs_choose_blks(fs);        /* Pick blocks to write during this cycle */
	fs_write_blks(fs);         /* Actually write the blocks */
	fs_sync(fs);               /* Synchronize and switch superblock */
	fs_verify_blocks(fs);      /* Verify blocks used by the now current snapshot */
	fs->sblk->hdr.seqno += 2U;
	fs_free_blks(fs);          /* Release blocks used by the now current snapshot */
}

/*!
 * @brief  Execute the test.
 *
 * @param [inout]  fs   "File system" to operate on.
 * @param [in]     dvc  The device to work with.
 */
static void
fs_test(fs_t *const fs, dvc_t *const dvc) {
	/* Do basic setup and verify potential remains of previous test */
	fs_init(fs, dvc);
	fs_verify(fs);

	/* Reset both superblocks on disk */
	fs_write_sblk(fs, 0U);
	fs_write_sblk(fs, 1U);

	/* Run test continuously */
	unsigned ntests = 0U;
	for(;;) {
		fs_test_cycle(fs);
		ntests++;
		printf("%u cycles successful\n", ntests);
	}
}


/****************************************************************************
	MAIN PROGRAM
 ****************************************************************************/

/*!
 * @brief  Main entrance.
 *
 * @param [in]  argc  Argument count. Should be 2.
 * @param [in]  argv  Arguments vector.
 *
 * @return  EXIT_SUCCESS. If runtime errors occur, other functions will
 *          immediately exit() with status EXIT_FAILURE.
 */
int
main(int const argc, char *argv[]) {
	if(argc != 2) {
		usage();
	}
	static dvc_t  dvc;
	dvc_open(&dvc, argv[1]);
	static fs_t  fs;
	fs_test(&fs, &dvc);
	return EXIT_SUCCESS;
}

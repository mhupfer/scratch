#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <uchar.h>

// qcc test_cxxrtomb.c -o test_cxxrtomb -Wall -D_FORTIFY_SOURCE=2 -O2 -L ~/mainline/stage/nto/x86_64/lib/

typedef struct {
	char16_t	lead_surrogate;
	mbstate_t	c32_mbstate;
} _Char16State;



size_t
freebsd_c16rtomb_l(char * __restrict s, char16_t c16, mbstate_t * __restrict ps/*, locale_t locale*/ )
{
	_Char16State *cs;
	char32_t c32;

	// FIX_LOCALE(locale);
	// if (ps == NULL)
	// 	ps = &(XLOCALE_CTYPE(locale)->c16rtomb);
	cs = (_Char16State *)ps;

	/* If s is a null pointer, the value of parameter c16 is ignored. */
	if (s == NULL) {
		c32 = 0;
	} else if (cs->lead_surrogate >= 0xd800 &&
	    cs->lead_surrogate <= 0xdbff) {
		/* We should see a trail surrogate now. */
		if (c16 < 0xdc00 || c16 > 0xdfff) {
			errno = EILSEQ;
			return ((size_t)-1);
		}
		c32 = 0x10000 + ((cs->lead_surrogate & 0x3ff) << 10 |
		    (c16 & 0x3ff));
	} else if (c16 >= 0xd800 && c16 <= 0xdbff) {
		/* Store lead surrogate for next invocation. */
		cs->lead_surrogate = c16;
		return (0);
	} else {
		/* Regular character. */
		c32 = c16;
	}
	cs->lead_surrogate = 0;

	return c32rtomb(s, c32, &cs->c32_mbstate);
}

size_t
freebsd_c16rtomb(char * __restrict s, char16_t c16, mbstate_t * __restrict ps)
{

	return (freebsd_c16rtomb_l(s, c16, ps/*, __get_locale()*/));
}

//#define convert()


int main(int argc, char* argv[]) {
    char s[6];
    mbstate_t ps;
    unsigned long *vinput = NULL;
	unsigned u;

    if (argc > 1) {
		vinput = calloc(argc - 1, sizeof(unsigned long));

		if (vinput) {
			for (u = 0; u < argc - 1; u++) {
        		vinput[u] = strtoul(argv[u+1], NULL, 0);
			}
		} else {
			printf("calloc failed\n");
			exit(1);
		}
    } else {
		printf("no arguments");
		exit(1);
	}

    memset(&ps, 0, sizeof(ps));
	//printf("bos=%ld\n", __builtin_object_size(s, __BOS_TYPE_INNER));
    size_t bytes = c32rtomb(s, vinput[0], &ps);
    printf("c32rtomb(%lx) : bytes=%ld errno=%d\n", vinput[0], bytes, errno);

    memset(&ps, 0, sizeof(ps));

	for (u = 0; u < argc - 1; u++) {
        bytes = c16rtomb(s, vinput[u], &ps);

		switch (bytes)
		{
		case -1:
			printf("c16rtomb(%lx): bytes=%ld errno=%d\n", vinput[u], bytes, errno);
			break;
		case 0:
			if (u + 1 == argc -1) {
				/* last input */
				printf("c16rtomb(%lx): bytes=%ld: error since last input\n", vinput[u], bytes);
			} else {
				printf("c16rtomb(%lx): bytes=%ld: check next input\n", vinput[u], bytes);
			}
			break;
		default:
			printf("c16rtomb(%lx): bytes=%ld \n", vinput[u], bytes);
			break;
		}
	}
    
    memset(&ps, 0, sizeof(ps));

    do {
        bytes = freebsd_c16rtomb(s, vinput[0], &ps);
    } while (bytes == 0);
    
    printf("freebsd_c16rtomb(%lx): bytes=%ld errno=%d\n", vinput[0], bytes, errno);

    return 0;
}

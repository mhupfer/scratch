/**
 * @brief stupid resmgr
 * @author Michael Hupfer
 * @email mhupfer@blackberry.com
 * creation date 2025/01/30
*/

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/resmgr.h>
#include <sys/trace.h>
// qcc test_exclusive_attach.c -o test_exclusive_attach -Wall -g -L ~/mainline/stage/nto/x86_64/lib/ -I ~/mainline/stage/nto/usr/include/


#define failed(f, e) printf("%s: %s() failed: %s\n", __func__, #f, strerror(e))


#define PASSED  "\033[92m"
#define FAILED "\033[91m"
#define ENDC  "\033[0m"

#define test_failed {\
    printf(FAILED"Test FAILED"ENDC" at %s, %d\n", __func__, __LINE__);\
    exit(1);\
}

#define DEFAULT_PATH "/dev/test_exclusive"
#define ONE_ABOVE "/dev"  
#define ONE_BELOW "/dev/test_exclusive/xxx"

/********************************/
/* main                         */
/********************************/
int main(int argc, char *argv[]) {
    int     ret = 0;

    iofunc_attr_t attr;
    iofunc_attr_init(&attr, S_IRUSR  | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, NULL, NULL);
    attr.uid = getuid();
    attr.gid = getgid();

    dispatch_t *dpp;
    dpp = dispatch_create_channel(-1, DISPATCH_FLAG_NOLOCK);

    if (dpp == NULL) {
        failed(dispatch_create_channel, errno);
    	return EXIT_FAILURE;
    }

    resmgr_connect_funcs_t      connect_funcs;
    resmgr_io_funcs_t           io_funcs;

    iofunc_func_init (_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                      _RESMGR_IO_NFUNCS, &io_funcs);

    resmgr_attr_t   rattr;
    memset (&rattr, 0, sizeof (rattr));

    /* first attach: attach DEFAULT_PATH non exclusive */
    int id1 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id1 == -1 )
    {
    	failed("resmgr_attach", errno);
    	return EXIT_FAILURE;
    }

    /* attach DEFAULT_PATH exclusive, should fail */
    int id2 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, _RESMGR_FLAG_EXCLUSIVE,
                    &connect_funcs, &io_funcs, &attr);

    if (id2 != -1 || errno != EEXIST) {
        test_failed;
    }

    /* attach DEFAULT_PATH non-exclusive, should succeed */
    id2 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id2 == -1) {
        test_failed;
    }

    if(resmgr_detach(dpp, id2, 0) == -1) {
    	failed("resmgr_detach", errno);
    	return EXIT_FAILURE;
    }

    if(resmgr_detach(dpp, id1, 0) == -1) {
    	failed("resmgr_detach", errno);
    	return EXIT_FAILURE;
    }

    /* first attach: attach DEFAULT_PATH exclusive */
    id1 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, _RESMGR_FLAG_EXCLUSIVE,
                    &connect_funcs, &io_funcs, &attr);

    if (id1 == -1 )
    {
    	failed("resmgr_attach", errno);
    	return EXIT_FAILURE;
    }

    /* attach DEFAULT_PATH exclusive, should fail */
    id2 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, _RESMGR_FLAG_EXCLUSIVE,
                    &connect_funcs, &io_funcs, &attr);

    if (id2 != -1 || errno != EEXIST) {
        test_failed;
    }

    /* attach DEFAULT_PATH non-exclusive, should fail */
    id2 = resmgr_attach(dpp, &rattr, DEFAULT_PATH, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id2 != -1 || errno != EEXIST) {
        test_failed;
    }

    /* attach above, should succeed */
    id2 = resmgr_attach(dpp, &rattr, ONE_ABOVE, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id2 == -1) {
        test_failed;
    }

    /* attach below, should succeed */
    int id3 = resmgr_attach(dpp, &rattr, ONE_BELOW, _FTYPE_ANY, 0,
                    &connect_funcs, &io_funcs, &attr);

    if (id3 == -1) {
        test_failed;
    }

    //skip all detaches

    printf(PASSED"Test PASS"ENDC"\n");

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

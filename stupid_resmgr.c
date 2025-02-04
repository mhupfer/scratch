/**
 * @brief stupid resmgr
 * @author Michael Hupfer
 * @email mhupfer@blackberry.com
 * creation date 2025/01/30
*/

// #include "procfs2mgr.h"
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
// qcc stupid_resmgr.c -o stupid_resmgr -Wall -g -L ~/mainline/stage/nto/x86_64/lib/ -I ~/mainline/stage/nto/usr/include/


#define failed(f, e) printf("%s: %s() failed: %s\n", __func__, #f, strerror(e))


/********************/
/* procfs2mgr_t     */
/********************/
typedef struct {
    struct {
        unsigned                    attach_flags;
        dispatch_t                  *dpp;
        resmgr_connect_funcs_t      connect_funcs;
        resmgr_io_funcs_t           io_funcs;
        dispatch_context_t          *ctp;
    } resmgr;
} stupid_resmg_r;


stupid_resmg_r    glob = {0};

static int init_resmgr(char *path, stupid_resmg_r *gl);
static void print_usage();
static int get_opts(int argc, char **argv, stupid_resmg_r *gl);

#define DEFAULT_PATH "/dev/stupid"


/********************************/
/* main                         */
/********************************/
int main(int argc, char *argv[]) {
    int     ret = 0;
    char    *path = DEFAULT_PATH;

    if (get_opts(argc, argv, &glob) != EOK) {
        return EXIT_FAILURE;
    }

    if (argc > 1) {
        if (argv[argc - 1][0] != '-') {
            path = argv[argc - 1];
        }
    }

    ret = init_resmgr(path, &glob);

    if (ret == 0) {
        while (1) {
            if (dispatch_block (glob.resmgr.ctp) == NULL) {
                failed(dispatch_block, errno);
                break;
            }
            if (dispatch_handler(glob.resmgr.ctp) == -1) {
                failed(dispatch_handler, errno);
            }
        }
    }

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}


/********************************/
/* init_resmgr                  */
/********************************/
static int init_resmgr(char *path, stupid_resmg_r *gl) {

    iofunc_attr_t  *attr;
    resmgr_attr_t   rattr;

    attr = calloc(1, sizeof *attr);

    if (attr == NULL) {
        return ENOMEM;
    }


    if (gl->resmgr.attach_flags & _RESMGR_FLAG_DIR) {
        iofunc_attr_init(attr, S_IFDIR | S_IRUSR  | S_IXUSR| S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, NULL, NULL);
    } else {
        iofunc_attr_init(attr, S_IRUSR  | S_IWUSR| S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, NULL, NULL);
    }
    attr->uid = getuid();
    attr->gid = getgid();

    gl->resmgr.dpp = dispatch_create_channel(-1, DISPATCH_FLAG_NOLOCK);

    if (gl->resmgr.dpp == NULL) {
        failed(dispatch_create_channel, errno);
        free(attr);
    	return -1;
    }

    iofunc_func_init (_RESMGR_CONNECT_NFUNCS, &gl->resmgr.connect_funcs,
                      _RESMGR_IO_NFUNCS, &gl->resmgr.io_funcs);


    memset (&rattr, 0, sizeof (rattr));
    rattr.msg_max_size = sysconf(_SC_PAGESIZE);

    trace_logf(1, "%s:", "resmgr_attach");
    if (resmgr_attach(gl->resmgr.dpp, &rattr, path, _FTYPE_ANY, gl->resmgr.attach_flags,
                    &gl->resmgr.connect_funcs, &gl->resmgr.io_funcs, attr) == -1 )
    {
    	failed("resmgr_attach", errno);
        free(attr);
    	return -1;
    }
                    
    gl->resmgr.ctp = dispatch_context_alloc(gl->resmgr.dpp);

    if (gl->resmgr.ctp == NULL)
    {
    	failed("dispatch_context_alloc", errno);
        free(attr);
    	return -1;
    }

    return 0;
}


/********************************/
/* get_opts                     */
/********************************/
static int get_opts(int argc, char **argv, stupid_resmg_r *gl) {
    int result = 0;
    while (optind < argc) {
        int c = getopt(argc, argv, "dx");

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'd':
                gl->resmgr.attach_flags |= _RESMGR_FLAG_DIR;
                break;
            case 'x':
                gl->resmgr.attach_flags |= _RESMGR_FLAG_EXCLUSIVE;
                break;
            case 'h':
                print_usage();
                exit(0);
            default:
                fprintf(stderr, "Unknown option: %c\n", c);
                result = -1;
                errno = EINVAL;
                break;
        }
    }

    return result;
}

/********************************/
/* print_usage                  */
/********************************/
static void print_usage() {
    printf("stupid_resmgr [-d] <path>\n");
    printf("\t-d    attach directoy resmgr\n");
    printf("\t-x    attach exclusive\n");
    printf("\t-h    print help message\n");
    printf("\t<path> resgmr path to create. Default /dev/stupid\n");
}
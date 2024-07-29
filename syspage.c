
#include <stdlib.h>
#include <stdio.h>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <errno.h>


struct syspage_entry *
load_syspage(int fd, int full, int *cross_endian);

int main() {
    char     path[64];
	sprintf( path, "/proc2/1/as" );
	int fd = open(path, O_RDONLY );
	if ( fd == -1 ) {
		perror("open");
		return 1;
	}

    int syspage_cross_endian;
	struct syspage_entry *mysyspage = load_syspage(fd, 0, &syspage_cross_endian);
	close(fd);

    if (mysyspage)
        printf("success\n");
}

struct syspage_entry *
load_syspage(int fd, int full, int *cross_endian) {
	struct syspage_entry	*ptr;
	int						len, err;
	*cross_endian = 0;

    printf("1st devctl\n");
	if ((err = devctl(fd, DCMD_PROC_SYSINFO, NULL, 0, &len)) == EOK) {
		if ((ptr = (struct syspage_entry *)malloc(len)) != NULL) {
            printf("2nd devctl\n");
			if ((err = devctl(fd, DCMD_PROC_SYSINFO, ptr, len, NULL)) == EOK) {
                printf("3rd devctl\n");
				if ((err = devctl(fd, -1, NULL, 0, NULL)) != EENDIAN || !full) {
					*cross_endian = (err == EENDIAN);
					return(ptr);
				}
			}
			free(ptr);
		}
		else {
			err = ENOMEM;
		}
	}
	errno = err;
	return(NULL);
}

#include <stdlib.h>
#include <unistd.h>

__attribute__ ((constructor))
static void pre_chroot()
{
	char const * const path = getenv("CHROOT_PATH");
	if (path != NULL) {
		chroot(path);
	}
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <path> [command]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	char const * const path = argv[1];

	if (chroot(path) == -1) {
		perror("chroot");
		exit(EXIT_FAILURE);
	}

	char *sh_argv[] = { "sh", NULL };
	argv = (argc >= 3) ? &argv[2] : sh_argv;
	execvp(argv[0], argv);

	fprintf(stderr, "exec(%s): %s", argv[0], strerror(errno));
	return EXIT_FAILURE;
}

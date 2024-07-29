#include <stdio.h>
#include <unistd.h>

int
main()
{
    printf("%d\n", getsid(0));
    char buffer[88];
    gethostname(buffer, sizeof(buffer));
    return 0;
}


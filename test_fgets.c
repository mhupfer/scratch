#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char * argv[]) {

    int bytes_to_read = 1;

    // if (argc > 1) {
    //     bytes_to_read = atoi(argv[1]);
    // }

    char s[16] ={42, 43, 42, 32, 42, 32, 42, 32, 42, 32, 42, 32, 42, 32, 42, 32};
    char *p = fgets(s, bytes_to_read, stdin);
    assert(p == s);
    assert(s[0] == '\0');
    assert(s[1] == 43);
    printf("PASS\n");
}
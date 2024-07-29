#include <stdlib.h>
#include <termios.h>
#include <assert.h>
#include <stdio.h>

// compile with stage
// qcc -Wall cfsetspped.c -o cfsetspped -I ~/mainline/stage/nto/usr/include/ -L ~/mainline/stage/nto/x86_64/lib/

int main(int argc, char * argv[])
{
	struct termios test_termios;
	speed_t newspeed = B300;

	test_termios.c_ospeed = B600;	// Manually setting c_ospeed as 600
	test_termios.c_ispeed = B600;  	// same for ispeed


	int rv = cfsetspeed( &test_termios, newspeed );
	assert(rv == 0);
	assert(test_termios.c_ospeed == B300);
	assert(test_termios.c_ispeed == B300);

    printf("Pass\n");

	return 0;
}

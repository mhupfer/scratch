#include <assert.h>
#include <time.h>
#include <wchar.h>

int main(int argc, char * argv[]) {
    struct tm tm = {
	.tm_sec = 59,
	.tm_min = 59,
	.tm_hour = 23,
	.tm_mday = 31,
	.tm_mon = 11,
	.tm_year = 99,
	.tm_wday = 5,
	.tm_yday = 364,
	.tm_isdst = 0
    };
    wchar_t buf[128];
    size_t len = wcsftime(buf, (size_t) 10, L"%C %D %e", &tm);
    assert(len == 0);
}

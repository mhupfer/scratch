#include <stdio.h>
#include <langinfo.h>
#include <time.h>

int main() {
    printf("%s\n", nl_langinfo(D_T_FMT));
    printf("%s\n", nl_langinfo(D_FMT));
    printf("%s\n", nl_langinfo(T_FMT));
    printf("%s\n", nl_langinfo( T_FMT_AMPM));
    printf("%s\n", nl_langinfo(ERA_D_T_FMT));
    printf("%s\n", nl_langinfo(ERA_D_FMT));
    printf("%s\n", nl_langinfo(ERA_T_FMT));

    time_t time_of_day;
    char buffer[ 180 ];

    time_of_day = time( NULL );
    int res = strftime( buffer, 180, "Today is %c",
           localtime( &time_of_day ) );
    printf("%d    %s\n", res, buffer );

    struct tm tm;
    char *x = strptime("Today is Mon May 15 11:13:01 1988", "Today is %c", &tm);
    printf("strptime returned %p\n", x);
}
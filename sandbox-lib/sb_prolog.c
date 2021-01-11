#include <stdio.h>

int 
sb_prolog(const char *uid) 
{
    printf("sandbox %s: call prolog\n", uid);
    return 1;
}

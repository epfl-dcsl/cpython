#include <stdio.h>

int 
sb_epilog(const char *uid) 
{
    printf("sandbox %s: call epilog\n", uid);
    return 1;
}

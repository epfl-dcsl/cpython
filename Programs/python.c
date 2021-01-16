/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include "smalloc.h"
#include "liblitterbox.h"

#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    return Py_Main(argc, argv);
}
#else
int
main(int argc, char **argv)
{
    /* (elsa) ADDED THIS */
    int ret;
    /*(aghosn) init the dynamic backend, reads the env-var from go.*/
    SB_Initialize();
    register_region = &SB_RegisterRegion; 
    register_growth = &SB_RegisterGrowth;
    if (!sm_pools_init(100, 10, sysconf(_SC_PAGESIZE))) {
        fprintf(stderr, "Error initializing the memory pools\n");
        return 1;
    }

    ret = Py_BytesMain(argc, argv);

    sm_release_pools();
    return ret;
}
#endif

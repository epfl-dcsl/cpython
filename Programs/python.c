/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include "pycore_object.h"
#include "mh_api.h"
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
    register_id = &SB_RegisterPackageId; 
    register_growth = &SB_AddSection;
    register_mh_refcount = &SB_checkref;
    register_ref = &mh_refcounter;
    mh_heaps_init(); 
    ret = Py_BytesMain(argc, argv);
    return ret;
}
#endif

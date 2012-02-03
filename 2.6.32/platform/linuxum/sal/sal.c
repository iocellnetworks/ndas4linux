#include <assert.h>
#include <unistd.h>
#include "sal/sal.h"
#include "sal/net.h"
#include "sal/debug.h"
#include "sal/types.h"

/* Assertion fail when platform dependency parameter is not valid */
void SalCheckPlatformValidity(void)
{    
    /* Check net */
    sal_assert(sizeof(sal_ether_header)==14);
    sal_assert(sizeof(xint8)==1);
    sal_assert(sizeof(xint16)==2);
    sal_assert(sizeof(xint32)==4);
    sal_assert(sizeof(xint64)==8);
    sal_assert(sizeof(xuint8)==1);
    sal_assert(sizeof(xuint16)==2);    
    sal_assert(sizeof(xuint32)==4);    
    sal_assert(sizeof(xuchar)==1);    
}

NDAS_SAL_API
void sal_init(void)
{
    SalCheckPlatformValidity();
    sal_net_init();
}
NDAS_SAL_API
void sal_cleanup(void)
{
    sal_net_cleanup();
    sal_mem_display(1);
}

NDAS_SAL_API ndas_error_t sal_gethostname(char* name, int size)
{
    if ( gethostname(name,size) ) 
         return NDAS_ERROR_INVALID_PARAMETER;
    return NDAS_OK;
}


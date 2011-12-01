// $Header$
/********************************************************************************
 * smapx.h
 * Coding Style follows that of original smap IRX author.
 ********************************************************************************/
#ifndef _IOP_SMAPX_H_
#define _IOP_SMAPX_H_
#include <tamtypes.h>
//#include <irx.h>

#define smapx_IMPORTS_start DECLARE_IMPORT_TABLE(smapx, 1, 1)
#define smapx_IMPORTS_end END_IMPORT_TABLE
#define I_SMapRegisterInputHook DECLARE_IMPORT(4, SMapRegisterInputHook)

typedef int (*SMAP_LOW_LEVEL_INPUT_HOOK)(struct pbuf*, struct netif*);

/**
 * SMapRegisterInputHook - register the hook function to handle 
 * the incoming network packet
 */
SMAP_LOW_LEVEL_INPUT_HOOK SMapRegisterInputHook(SMAP_LOW_LEVEL_INPUT_HOOK hook);


#endif


#ifndef __MIO_MODULES_H__
#define __MIO_MODULES_H__
#include <_ansi.h>
#include <tamtypes.h>
#include <mio/features.h>

/**        modules.h - Define module ids for debugging.
 *
 *        This header file contains definitions for modules used in MIO. These definitions
 *        are used by the debug/trace facility.
 */

typedef enum _MioModuleID {
    MIO_MODULE_EXECVE_CC        = 1,
    MIO_MODULE_ELFLOADER_CC        = 2,
    MIO_MODULE_SIMPLE_FILEIO_CC    = 3,
    MIO_MODULE_SIGNING_CC        = 4,
    MIO_MODULE_SECURITY_CC        = 5,
    MIO_MODULE_VFORK_CC            = 6,
    MIO_MODULE_STAT_CC            = 7,
    MIO_MODULE_UPDATE_CC        = 8,
    MIO_MODULE_PS2CLOCK_CC        = 9,

    MIO_MODULE_USBENDPOINT_C    = 10,
    MIO_MODULE_USBCONFIG_C        = 11,
    MIO_MODULE_USBINTERFACE_C    = 12,
    MIO_MODULE_USBIO_C            = 13,
    MIO_MODULE_BCACHE_C            = 14,
    MIO_MODULE_CBW_C            = 15,
    MIO_MODULE_CSW_C            = 16,
    MIO_MODULE_DBLIST_C            = 17,
    MIO_MODULE_FAT_C            = 18,
    MIO_MODULE_FATMAN_C            = 19,
    MIO_MODULE_SCSI2_C            = 20,
    MIO_MODULE_SEMA_C            = 21,
    MIO_MODULE_SFF8070I_C        = 22,
    MIO_MODULE_SPC2_C            = 23,
    MIO_MODULE_STORAGE_C        = 24,
    MIO_MODULE_TRACEFS_C        = 25,
    MIO_MODULE_UFI_C            = 26,
    MIO_MODULE_UTILS_C            = 27,
    MIO_MODULE_UTIL_FUNCS_C        = 28,
    MIO_MODULE_DELIMITTED_BUFFER_CC = 29,
    MIO_MODULE_MEDIO_CONFIG_CC    = 30,
    MIO_MODULE_MEDIO_MAIN_CC    = 31,
    MIO_MODULE_RMS_CC            = 32,
    MIO_MODULE_PSEUDOPROCS_CC    = 33,
    MIO_MODULE_EVENT_BROADCASTER_CC = 34,
    MIO_MODULE_ROMFS_C            = 35,

    MIO_MODULE_MAX                = 80 // maximum module id allowed (see modules.c)
} MioModuleID;


#if defined(__cplusplus)
class MioModule {
public:
    MioModule(MioModuleID, const char *moduleName, u64 featureIDs);

    int isTracing() const;
    int setTracing(int onOff);
    const char *getModuleName() const { return moduleName; };

    static int setTracing(MioModuleID moduleID, int onOff);

private:
    MioModuleID    moduleID; // module identifier
    const char *moduleName;
    u64 featureIDs; // bit vector of features that this module belongs to
};

#else

typedef struct _MioModule {
    MioModuleID    moduleID; // module identifier
    const char *moduleName;
    u64 featureIDs; // bit vector of features that this module belongs to
} MioModule;
#endif

_BEGIN_STD_C
extern int MioModuleIsTracing(const MioModule *self);
extern int MioModuleSetTracing(MioModuleID moduleID, int onOff);
_END_STD_C

#endif // __MIO_MODULES_H__

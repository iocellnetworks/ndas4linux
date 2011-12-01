
#ifndef __MIO_FEATURES_H__
#define __MIO_FEATURES_H__
#include <_ansi.h>


/**        features.h - Define feature ids for debugging.
 *
 *        This header file contains definitions for features used in MIO. These definitions
 *        are used by the debug/trace facility.
 */

typedef enum _MioFeatureID {
    MIO_FEATURE_SLOADER            = 10,
    MIO_FEATURE_PLAYLIST        = 11,
    MIO_FEATURE_MP3_PLAYER        = 12,
    MIO_FEATURE_OGG_PLAYER        = 13,
    MIO_FEATURE_VIDEO_PLAYER    = 14,
    MIO_FEATURE_LOGIN            = 15,
    MIO_FEATURE_IMAGEVIEWER        = 16,
    MIO_FEATURE_USBMS            = 17,
    MIO_FEATURE_FATFS            = 18,
    MIO_FEATURE_USB                = 19,
    MIO_FEATURE_STORAGE            = 20,

    MIO_FEATURE_MAX                = 64 // maximum number of features allowed (see features.c)
} MioFeatureID;

#define BIT(b) (1 << b)

_BEGIN_STD_C
extern int MioFeatureIsTracing(MioFeatureID featureID);
extern void MioFeatureSetTracing(MioFeatureID featureID, int onOff);
_END_STD_C

#endif // __MIO_FEATURES_H__

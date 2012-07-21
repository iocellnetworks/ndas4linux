/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/
/* Create to use gtypes.h */
#ifndef _GLIBCONFIG_H_
#define _GLIBCONFIG_H_ 

#include "sal/types.h"

#define G_BEGIN_DECLS
#define G_END_DECLS
#define G_GNUC_CONST

typedef xint8 gint8;
typedef xuint8 guint8;
typedef xint16 gint16;
typedef xuint16 guint16;
#define G_GINT16_MODIFIER "h"
#define G_GINT16_FORMAT "hi"
#define G_GUINT16_FORMAT "hu"
typedef xint32 gint32;
typedef xuint32 guint32;
#define G_GINT32_MODIFIER ""
#define G_GINT32_FORMAT "i"
#define G_GUINT32_FORMAT "u"

#ifdef __LITTLE_ENDIAN_BYTEORDER

#define GINT16_TO_LE(val)       ((gint16) (val))
#define GUINT16_TO_LE(val)      ((guint16) (val))
#define GINT16_TO_BE(val)       ((gint16) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_BE(val)      (GUINT16_SWAP_LE_BE (val))
#define GINT32_TO_LE(val)       ((gint32) (val))
#define GUINT32_TO_LE(val)      ((guint32) (val))
#define GINT32_TO_BE(val)       ((gint32) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_BE(val)      (GUINT32_SWAP_LE_BE (val))
#define GINT64_TO_LE(val)       ((gint64) (val))
#define GUINT64_TO_LE(val)      ((guint64) (val))
#define GINT64_TO_BE(val)       ((gint64) GUINT64_SWAP_LE_BE (val))
#define GUINT64_TO_BE(val)      (GUINT64_SWAP_LE_BE (val))

#if 0
#define GLONG_TO_LE(val)        ((glong) GINT32_TO_LE (val))
#define GULONG_TO_LE(val)       ((gulong) GUINT32_TO_LE (val))
#define GLONG_TO_BE(val)        ((glong) GINT32_TO_BE (val))
#define GULONG_TO_BE(val)       ((gulong) GUINT32_TO_BE (val))
#endif /* long is not always 32 bits */
#define GINT_TO_LE(val)         ((gint) GINT32_TO_LE (val))
#define GUINT_TO_LE(val)        ((guint) GUINT32_TO_LE (val))
#define GINT_TO_BE(val)         ((gint) GINT32_TO_BE (val))
#define GUINT_TO_BE(val)        ((guint) GUINT32_TO_BE (val))
#define G_BYTE_ORDER G_LITTLE_ENDIAN

#else

#define GINT16_TO_LE(val)       ((gint16) GUINT16_SWAP_LE_BE (val))
#define GUINT16_TO_LE(val)      (GUINT16_SWAP_LE_BE (val))
#define GINT16_TO_BE(val)       ((gint16) (val))
#define GUINT16_TO_BE(val)      ((guint16) (val))
#define GINT32_TO_LE(val)       ((gint32) GUINT32_SWAP_LE_BE (val))
#define GUINT32_TO_LE(val)      (GUINT32_SWAP_LE_BE (val))
#define GINT32_TO_BE(val)       ((gint32) (val))
#define GUINT32_TO_BE(val)      ((guint32) (val))
#define GINT64_TO_LE(val)       ((gint64) GUINT64_SWAP_LE_BE (val))
#define GUINT64_TO_LE(val)      (GUINT64_SWAP_LE_BE (val))
#define GINT64_TO_BE(val)       ((gint64) (val))
#define GUINT64_TO_BE(val)      ((guint64) (val))

#if 0
#define GLONG_TO_LE(val)        ((glong) GINT32_TO_BE (val))
#define GULONG_TO_LE(val)       ((gulong) GUINT32_TO_LE (val))
#define GLONG_TO_BE(val)        ((glong) GINT32_TO_LE (val))
#define GULONG_TO_BE(val)       ((gulong) GUINT32_TO_BE (val))
#endif /* long i snot always 32 bits */
#define GINT_TO_LE(val)         ((gint) GINT32_TO_LE (val))
#define GUINT_TO_LE(val)        ((guint) GUINT32_TO_LE (val))
#define GINT_TO_BE(val)         ((gint) GINT32_TO_BE (val))
#define GUINT_TO_BE(val)        ((guint) GUINT32_TO_BE (val))
#define G_BYTE_ORDER G_BIG_ENDIAN

#endif
/* TODO: PDP endian if need */

#endif /* _XLIB_GLIBCONFIG_H_ */


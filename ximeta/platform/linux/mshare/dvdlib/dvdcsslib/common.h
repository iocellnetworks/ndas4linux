/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: common.h,v 1.5 2003/04/11 10:00:29 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/
#ifdef ILGU_LINUX
	#include <stdint.h>
#else
    /* Fallback types (very x86-centric, sorry) */
    typedef unsigned char       uint8_t;
    typedef signed char         int8_t;
    typedef unsigned int        uint32_t;
    typedef signed int          int32_t;
#endif

#ifdef ILGU_WINDOWS
#   ifndef PATH_MAX
#   define PATH_MAX MAX_PATH
#   endif
#endif




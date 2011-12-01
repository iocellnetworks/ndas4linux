#ifndef BSWAP_H_INCLUDED
#define BSWAP_H_INCLUDED

/*
 * Copyright (C) 2000, 2001 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#if defined(WORDS_BIGENDIAN)
/* All bigendian systems are fine, just ignore the swaps. */  
#define B2N_16(x) (void)(x)
#define B2N_32(x) (void)(x)
#define B2N_64(x) (void)(x)

#else 


#ifdef ILGU_LINUX
#include <sys/param.h>
#include <byteswap.h>
#define B2N_16(x) x = bswap_16(x)
#define B2N_32(x) x = bswap_32(x)
#define B2N_64(x) x = bswap_64(x)

#endif

#endif /* WORDS_BIGENDIAN */

#endif /* BSWAP_H_INCLUDED */

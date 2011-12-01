/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _NDAS_GUI_DEFS_
#define _NDAS_GUI_DEFS_

#ifdef DEBUG
extern "C" {
#include <stdio.h>
}
#define debug(x...) do {  \
    fprintf(stderr,"%s|", __FUNCTION__); \
    fprintf(stderr,x); \
    fprintf(stderr, "|%s:%d\n", __FILE__, __LINE__); \
} while(0)
#else
#define debug(x...) do {} while(0)
#endif

#define INSET 20
#define FONT_HEIGHT 30
#define FONT_HEIGHT_GAP 10

#endif

/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/

#ifndef __XIXCORE_INTERNAL_TYPES_H__
#define __XIXCORE_INTERNAL_TYPES_H__

#define XIXCORE_OFFSETOF(TYPE, MEMBER) ((xc_size_t) &((TYPE *)0)->MEMBER)
#define XIXCORE_CONTAINEROF(PTR, TYPE, MEMBER) ((TYPE *)( \
                                                  (char *)(PTR) - \
                                                  XIXCORE_OFFSETOF(TYPE,MEMBER)))

#define XIXCORE_UNREFERENCED_PARAMETER(P)          (P);

#endif

/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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
#ifndef _NDAS_ID_H
#define _NDAS_ID_H

/**
 * Public Data Structure for ndas id and ndas key
 */
typedef struct ndas_id {
    unsigned char id1[5];
    unsigned char id2[5];
    unsigned char id3[5];
    unsigned char id4[5];
} ndas_id_t, *ndas_id_ptr;

typedef struct ndas_key {
    unsigned char key[5];
} ndas_key_t, *ndas_key_ptr;

typedef struct ndas_idkey {
    ndas_id_t id;
    ndas_key_t key;
} ndas_idkey_t, *ndas_idkey_ptr;

#endif //_NDAS_ID_H

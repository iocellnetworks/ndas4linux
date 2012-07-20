/*
 -------------------------------------------------------------------------
 Copyright (C) 2011, IOCELL Networks Corp. Plainsboro, NJ, USA.
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

#include "ndasuser/def.h"
#include "ndasuser/ndaserr.h"


#ifndef _XIXFS_EVENT_H__INCLUDED_
#define _XIXFS_EVENT_H__INCLUDED_

#ifdef XPLAT_XIXFS_EVENT_USER
#define LPX_NODE_LEN    6

struct sockaddr_lpx { 
   unsigned short   		 slpx_family; /* Address family. */
   unsigned short        	slpx_port;
   unsigned char            slpx_node[LPX_NODE_LEN];
} __attribute__((packed));

#endif //#ifdef XPLAT_XIXFS_EVENT_USER


#define XIXFS_SRVEVENT_PORT	(0x1000) 
#define XIXFS_MAX_DATA_LEN 	(256)

#ifdef XPLAT_XIXFS_EVENT

typedef void (*module_handle)(void * ctx);

typedef struct _XIXFS_EVENT_CTX
{
	// used by other module
	int IsSrvType;
	module_handle	packet_handler;
	module_handle	error_handler;
	void * Context;
	
	// used by lower event do not touch this
	int sockfd;
	char *buf;
	unsigned int sz;
	struct sockaddr_lpx	myaddr;
	struct sockaddr_lpx *dest;
	int running;

}XIXFS_EVENT_CTX, *PXIXFS_EVENT_CTX;





/* Description
 *  send xixfs event service
 * This function must be called before calling ndas_cleanup
 */
NDASUSER_API 
int xixfs_Event_Send(void *Ctx) ;

/* Description
 *  stop xixfs event service
 * This function must be called before calling ndas_cleanup
 */
NDASUSER_API 
int	xixfs_Event_clenup(void *Ctx);


/* Description
 *  start xixfs event service
 *		must set error_handler
 *		if server, must set packet_handler
 * This function must be called before calling ndas_cleanup
 */
NDASUSER_API 
int	xixfs_Event_init(void *Ctx);

/*
*/
NDASUSER_API 
int xixfs_IsLocalAddress(unsigned char *addr);
#endif //#ifdef XPLAT_XIXFS_EVENT
#endif //#ifndef _XIXFS_EVENT_H__INCLUDED_

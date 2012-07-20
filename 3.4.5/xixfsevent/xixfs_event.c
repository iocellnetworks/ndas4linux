#include "xplatcfg.h"
#include <sal/libc.h>
#include <sal/sync.h>
#include <sal/time.h>
#include <xlib/gtypes.h> // g_ntohl g_htonl g_ntohs g_htons
#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "xixfsevent/xixfs_event.h"

#ifdef XPLAT_XIXFS_EVENT


#ifndef XPLAT_SIO
#error XPLAT_SIO should be set to use XIXFS EVENT
#endif //#ifndef XPLAT_SIO

#ifdef DEBUG
#include "sal/debug.h"
#define    debug_xixfs_event(l, x...)    \
do {\
    if(l <= DEBUG_LEVEL_XIXFS_EVENT ) {     \
        sal_debug_print("XIXFS_EVENT|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define debug_xixfs_event(l, x...)	do{} while(0)
#endif // #ifdef DEBUG


ndas_error_t	xixfs_Event_reinit(PXIXFS_EVENT_CTX xevent);


NDASUSER_API 
int xixfs_Event_Send(void *Ctx) 
{
	PXIXFS_EVENT_CTX xevent = (PXIXFS_EVENT_CTX)Ctx;
	struct sockaddr_lpx bcast;
	ndas_error_t result = NDAS_OK;

	sal_assert(xevent);

	LPX_ADDR_INIT(&bcast, LPX_BROADCAST_NODE, XIXFS_SRVEVENT_PORT);
	result = lpx_sendto(xevent->sockfd, xevent->buf, xevent->sz, 0, &bcast, sizeof(bcast));

	if ( xevent->sockfd < 0 ) {
		xixfs_Event_reinit(xevent);
		return (int)NDAS_ERROR;
	}	

	return (int)result;
	 
}




void xixfs_srvEvent_handler(lpx_siocb* cb, struct sockaddr_lpx *from, void* arg)
{

	PXIXFS_EVENT_CTX xevent = (PXIXFS_EVENT_CTX)arg;
	


	debug_xixfs_event(5,"start xixfs_srvEvent_handler");
	
	debug_xixfs_event(5, "ing cb=%p from={%s:0x%x} arg=%p",
		cb,from ? SAL_DEBUG_HEXDUMP_S(from->slpx_node,SAL_ETHER_ADDR_LEN) :"NULL",
		from ? g_ntohs(from->slpx_port) : -1, arg);

	if ( !NDAS_SUCCESS(cb->result)) {
		debug_xixfs_event(0, "result=%d", cb->result);

		if ( xevent->running ) 
			xixfs_Event_reinit(xevent);
		return ;
	}
	
	if ( cb->buf !=xevent->buf ) {
		debug_xixfs_event(1, "INTERNAL ERROR cb->buf != v_hix_svr.buf");
		return ;
	}

	xevent->sz = cb->nbytes;     
	xevent->dest = from;

	debug_xixfs_event(8, "message from %s:0x%x" , 
		SAL_DEBUG_HEXDUMP_S(from->slpx_node,SAL_ETHER_ADDR_LEN),
		g_ntohs(from->slpx_port));


	if(xevent->packet_handler) {
		xevent->packet_handler((void *)xevent);
	}

	lpx_sio_recvfrom(
            xevent->sockfd, 
            xevent->buf, 
            XIXFS_MAX_DATA_LEN,
            SIO_FLAG_MULTIPLE_NOTIFICATION, 
            xixfs_srvEvent_handler,
            (void *)xevent, 
            0);	

	
	if ( xevent->sockfd < 0 ) {
		xixfs_Event_reinit(xevent);
		return ;
	}	

	
}




//define initialization 
NDASUSER_API 
int	xixfs_Event_clenup(void *Ctx)
{
	PXIXFS_EVENT_CTX xevent = (PXIXFS_EVENT_CTX)Ctx;
	sal_assert(xevent);
	
	debug_xixfs_event(5, "start xixfs_Event_clenup");

	xevent->running = 0;

	if( xevent->sockfd > 0 ) {
		lpx_close(xevent->sockfd);
		xevent->sockfd = -1;
	}

	if( xevent->buf ) {
		sal_free(xevent->buf);
		xevent->buf = NULL;		
	}
	
	debug_xixfs_event(5, "Success end xixfs_Event_clenup");

	return (int)NDAS_OK;
}






ndas_error_t	xixfs_Event_reinit(PXIXFS_EVENT_CTX xevent)
{
	ndas_error_t result = NDAS_OK;
	int fd = xevent->sockfd;
	int portnum = 0;
	
	sal_assert(xevent);
	
	debug_xixfs_event(5, "start xixfs_Event_reinit");

	if( fd > 0 ) {
		xevent->sockfd = -1;
		lpx_close(fd);
	}


	if(  xevent->buf ) {
		sal_free(xevent->buf);
		xevent->buf = NULL;
	}


	if(xevent->IsSrvType){
		portnum = XIXFS_SRVEVENT_PORT;
	}else{
		portnum = 0;
	}

	xevent->sockfd = lpx_socket(LPX_TYPE_DATAGRAM, 0); 
	debug_xixfs_event(3, "event  fd=%d", xevent->sockfd );
	
	if( !NDAS_SUCCESS(xevent->sockfd) ){
		debug_xixfs_event(0, "fail create socket : lpx_socket");
		return xevent->sockfd;
	}

	LPX_ADDR_INIT(&xevent->myaddr, 0, portnum); /* any address */
	
	result = lpx_bind(xevent->sockfd, &xevent->myaddr, sizeof(xevent->myaddr));
    	debug_xixfs_event(3, "LPX: bind: %d", result);

	if (!NDAS_SUCCESS(result))
	{
	    goto error_out;
	}
	
	xevent->buf = sal_malloc(XIXFS_MAX_DATA_LEN);
	if( !xevent->buf ) {
		result = NDAS_ERROR_OUT_OF_MEMORY;
		goto error_out;
	}

	
	if( xevent->IsSrvType ) {
		result = lpx_sio_recvfrom(
	            xevent->sockfd, 
	            xevent->buf, 
	            XIXFS_MAX_DATA_LEN,
	            SIO_FLAG_MULTIPLE_NOTIFICATION, 
	            xixfs_srvEvent_handler,
	            (void *)xevent, 
	            0);	

		if( !NDAS_SUCCESS(result) ){
			goto error_out;
		}
	}

	
	debug_xixfs_event(5, "Success end xixfs_Event_reinit");

	return NDAS_OK;
error_out:	
	xixfs_Event_clenup(xevent);

	if(xevent->error_handler) {
		xevent->error_handler((void *)xevent );
	}
	
	return result;
}







NDASUSER_API 
int	xixfs_Event_init(void *Ctx )
{

	PXIXFS_EVENT_CTX xevent = (PXIXFS_EVENT_CTX)Ctx;
	sal_assert(xevent);
	
	debug_xixfs_event(5, "start xixfs_Event_init");
	if(xevent->packet_handler){
		if(xevent->IsSrvType == 0 ){
			return (int)NDAS_ERROR;
		}
	}

	if(!xevent->error_handler) {
		return (int)NDAS_ERROR;
	}

	xevent->running = 1;
	
	xixfs_Event_reinit(xevent);
	debug_xixfs_event(5, "Success end xixfs_Event_init");

	return (int)NDAS_OK;
}



NDASUSER_API 
int xixfs_IsLocalAddress(unsigned char *addr)
{
	 return lpxitf_Is_local_node(addr);
}

#endif// #ifdef XPLAT_XIXFS_EVENT




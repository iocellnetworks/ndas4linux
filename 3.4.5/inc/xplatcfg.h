#include "dbgcfg.h"

/* 
	xbuf dynamic allocation when the static pool is exausted. Recommended to turn on.
*/
#define XPLAT_XBUF_DYNAMIC 

/* 
	Enable reconnection feature 
*/
#define XPLAT_RECONNECT

/*
	Do not send ack packet until packet other than ACK is sent or 50ms passes.
*/
#define USE_DELAYED_ACK     

/*
	Delay event-set until all data to be expected to received is ready.
	Deduce context switch so that deduce CPU usage.
*/
#define USE_DELAYED_RX_NOTIFICATION 	

/* 
	Copy user buffer to network buffer directly.
	This reduces memory copy but this option will use more system network buffer
*/
#define USE_XBUF_FROM_NET_BUF_AT_TX  

/* 
     Use system mem pool functions instead of xbuf's mem pool. 
     Reduce fragmentation and memory usage. Currently implemented on linux kernel mode only.
*/
#define USE_SYSTEM_MEM_POOL    

/*
	Increase/decrease number of request blocks to reduce network congestions.
*/
#define USE_FLOW_CONTROL

#ifdef EXPORT_LOCAL
#define LOCAL
#else
#define LOCAL static
#endif

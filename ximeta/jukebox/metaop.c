#ifdef XPLAT_JUKEBOX

#include "xplatcfg.h"
#include <sal/thread.h>
#include <sal/time.h>
#include <sal/sync.h>
#include <sal/mem.h>

#include <xlib/dpc.h>
#include <xlib/xbuf.h>

#include <netdisk/netdiskid.h>
#include <netdisk/ndasdib.h>
#include <netdisk/sdev.h>
#include <netdisk/conn.h>
#include "mediajukedisk.h"
#include <jukebox/diskmgr.h>
#include "cipher.h"
#include "metaop.h"
//#define DMETAOPDBG	


#define DEBUG_LEVEL_MSHARE_METAOP 5

#define    debug_metaop(l, x)    do {\
    if(l <= DEBUG_LEVEL_MSHARE_METAOP) {     \
        sal_debug_print("UD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println x;    \
    } \
} while(0)    




/* This function is used for asynchronous IO ndas_io called without done handler */
void juke_io_blocking_done(int slot, ndas_error_t err, void* arg)
{
    struct juke_io_blocking_done_arg* darg = (struct juke_io_blocking_done_arg*)arg;
    darg->err = err;
    sal_semaphore_give((sal_semaphore)darg->done_sema);
}


static int RawDiskSecureRWOp(
	logunit_t *sdev,
	int 		Command,
	unsigned long long 	start_sector,
	int		count,
	unsigned char *	 	buff
	)
{
#ifdef XPLAT_ASYNC_IO
    struct udev_request_block *tir = NULL;
    struct juke_io_blocking_done_arg darg;
#endif

    udev_t * udev = SDEV2UDEV(sdev);
    ndas_error_t err;
    ndas_io_request *ioreq;
    int ret;
#ifdef META_ENCRYPT
    struct nd_diskmgr_struct * diskmgr;
    struct media_key * MKey;
    sal_assert(udev);
    diskmgr= udev->Disk_info;
    sal_assert(diskmgr);
    MKey = (struct media_key *)diskmgr->MetaKey;
    sal_assert(MKey);
    debug_metaop(2,("POSE in RawDiskSecureRWOp diskmgr->MetaKey(%p)\n", diskmgr->MetaKey));    
#endif	

    if(Command == ND_CMD_WRITE)
    {
#ifdef META_ENCRYPT
	debug_metaop(5,("call encrtyption!\n"));
	EncryptBlock(
		(PNCIPHER_INSTANCE)MKey->cipherInstance,
		(PNCIPHER_KEY)MKey->cipherKey,
		count*SECTOR_SIZE,
		(xuint8*) buff, 
		(xuint8*) buff
		);		
#endif
    }

#ifdef XPLAT_ASYNC_IO
    debug_metaop(5,("call async_io!\n"));

    tir = tir_alloc(Command, NULL);
    tir->func = NULL;
    tir->arg = 0;

    ioreq = tir->req;
    ioreq->buf = (xint8 *)buff;
    ioreq->start_sec = start_sector;
    ioreq->num_sec = count;
    ioreq->done = juke_io_blocking_done;
    ioreq->done_arg = &darg;

    darg.done_sema = sal_semaphore_create("ndas_io_done", 1, 0);  
    sdev->ops->io(sdev, tir, FALSE); //uop_io

    do {
        ret = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
    } while (ret == SAL_SYNC_INTERRUPTED);

    sal_semaphore_give(darg.done_sema);
    sal_semaphore_destroy(darg.done_sema);
    debug_metaop(5, ("ed err=%d", darg.err));
    err = darg.err;
#else
   debug_metaop(5,("call sync_io!\n"));
   sal_assert(udev);

    if (udev->conn.writeshare_mode) {
        err = nwl_take(udev, 2000);
        if (!NDAS_SUCCESS(err)) 
            goto io_out;
    }    
    err = conn_rw(&udev->conn, Command,
        start_sector, count, buff);

io_out:
    if (udev->conn.writeshare_mode) {
        nwl_give(udev);
    }	
#endif

    if (!NDAS_SUCCESS(err)) {
        debug_metaop(1, ("conn_read_dib failed: %d", err));
        return err;
    }


#ifdef META_ENCRYPT
    debug_metaop(2,("Call DecryptBlock buff(%p)\n", buff)); 
	DecryptBlock(
		(PNCIPHER_INSTANCE)MKey->cipherInstance,
		(PNCIPHER_KEY)MKey->cipherKey,
		count*SECTOR_SIZE,
		(xuint8*) buff,
		(xuint8*) buff
		);	
#endif	
    
    return NDAS_OK;
 }




			
int GetDiskMeta(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISK_META 	phead;
	phead = (PON_DISK_META)buf;
#endif //DMETAOPDBG	
	if(count < 3) return 0;
debug_metaop(2,("_GetDiskMeta Enter %p\n", sdev));
	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,DISK_META_START,count,buf);
	if(ndStatus != NDAS_OK) return 0;

#ifdef DMETAOPDBG
debug_metaop(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DMETAOPDBG

debug_metaop(2,("---> _GetDiskMeta Leave %p\n", sdev));
	return 1;	
}



int GetDiskMetaMirror(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int start_sector;
#ifdef DMETAOPDBG
	PON_DISK_META 	phead;
	phead = (PON_DISK_META)buf;
#endif //DMETAOPDBG	
	if(count < DISK_META_READ_SECTOR_COUNT) return 0;

debug_metaop(2,("_GetDiskMetaMirror Enter %p\n", sdev));
	start_sector = DISK_META_MIRROR_START;	

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,start_sector, count, buf);
	if(ndStatus != NDAS_OK) return 0;
#ifdef DMETAOPDBG
debug_metaop(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DMETAOPDBG

debug_metaop(2,("---> _GetDiskMetaMirror Leave %p\n", sdev));
	return 1;	
}



int GetDiskLogHead(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISK_LOG_HEADER phead;
	phead = (PON_DISK_LOG_HEADER) buf;
#endif //DMETAOPDBG	
	if(count < 1) return 0;
debug_metaop(2,("_GetDiskLogHead Enter %p\n", sdev));


	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,DISK_LOG_START,count,buf);
	if(ndStatus != NDAS_OK) return 0;
#ifdef DMETAOPDBG
debug_metaop(2,("L-AGE(%d) L-INDEX(%d) L-ACTION(%d) L-HISTORY(%d)\n", phead->latest_age, phead->latest_index, phead->latest_log_action, phead->latest_log_history));
#endif //DMETAOPDBG

debug_metaop(2,("---> _GetDiskLogHead Leave %p\n", sdev));
	return 1;	
}





int GetDiskLogData( logunit_t * sdev, unsigned char * buf, int index, int count)
{
	unsigned int temp ;
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISK_LOG phead;
#endif //DMETAOPDBG
	if(count < 2) return 0;
	
debug_metaop(2,("_GetDiskLogData Enter %p : index %d\n", sdev, index));
	temp = index*DISK_LOG_DATA_SECTOR_COUNT;
	temp = temp + DISK_LOG_DATA_START;
	

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,temp,count,buf);
	if(ndStatus != NDAS_OK) return 0;

#ifdef DMETAOPDBG
	phead =(PON_DISK_LOG)(buf);
debug_metaop(2,("AGE(%d) INDEX(%d) ACTION(%d) VALID(%d)\n", phead->age, index, phead->action, phead->valid));
#endif //DMETAOPDBG
debug_metaop(2,("---> _GetDiskLogData Leave %p : index %d\n", sdev, index));
	return 1;	
	
}


int SetDiskMeta(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
	//unsigned char	*pfree;
#ifdef DMETAOPDBG
	PON_DISK_META phead;
	phead = (PON_DISK_META)buf;
#endif // DMETAOPDBG	
	if(count < 1) return 0;

debug_metaop(2,("_SetDiskMeta Enter %p\n", sdev));
#ifdef DMETAOPDBG
debug_metaop(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DMETAOPDBG

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_WRITE,DISK_META_START,count,buf);
	if(ndStatus != NDAS_OK) return 0;

debug_metaop(2,("---> _SetDiskMeta Leave %p\n", sdev));
	return 1;	
}




int SetDiskMetaMirror(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int	start_sector;
#ifdef DMETAOPDBG
	PON_DISK_META phead;
	phead = (PON_DISK_META)buf;
#endif //DMETAOPDBG	
	if(count < 1) return 0;

debug_metaop(2,("_SetDiskMetaMirror Enter %p\n", sdev));
#ifdef DMETAOPDBG
debug_metaop(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DMETAOPDBG

	start_sector = DISK_META_MIRROR_START;

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_WRITE,start_sector,count,buf);
	if(ndStatus != NDAS_OK) return 0;
debug_metaop(2,("---> _SetDiskMetaMirror Leave %p\n", sdev));
	return 1;	
}



int SetDiskLogHead(logunit_t * sdev, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISK_LOG_HEADER phead;
	phead = (PON_DISK_LOG_HEADER) buf;
#endif //DMETAOPDBG	
	if(count < 1) return 0;

debug_metaop(2,("_SetDiskLogHead Enter %p\n", sdev));
#ifdef DMETAOPDBG
debug_metaop(2,("L-AGE(%d) L-INDEX(%d) L-ACTION(%d) L-HISTORY(%d)\n", phead->latest_age, phead->latest_index, phead->latest_log_action, phead->latest_log_history));
#endif //DMETAOPDBG

	ndStatus = RawDiskSecureRWOp(sdev,ND_CMD_WRITE,DISK_LOG_START,count,buf);

	if(ndStatus != NDAS_OK) return 0;;
debug_metaop(2,("---> _SetDiskLogHead Leave %p\n", sdev));
	return 1;	
}





int SetDiskLogData( logunit_t * sdev, unsigned char * buf, int index, int count)
{
	unsigned int temp ;
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISK_LOG phead;
#endif //DMETAOPDBG
	if(count < 2) return 0;
	
debug_metaop(2,("_SetDiskLogData Enter %p : index %d\n", sdev, index));
#ifdef DMETAOPDBG
	phead = (PON_DISK_LOG)(buf);
debug_metaop(2,("AGE(%d) INDEX(%d) ACTION(%d) VALID(%d)\n", phead->age, index, phead->action, phead->valid));
#endif
	temp = index * DISK_LOG_DATA_SECTOR_COUNT;
	temp = temp + DISK_LOG_DATA_START;
	


	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_WRITE,temp,count,buf);
	if(ndStatus != NDAS_OK) return 0;

debug_metaop(2,("---> _SetDiskLogData Leave %p : index %d\n", sdev, index));
	return 1;	
	
}

/*
	Read / Write Disc information
	buf must be larger than (1<<(DISK_META_SECTOR_COUNT + SECTOR_SIZE_BIT ))
*/
int GetDiscMeta(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISC_ENTRY phead;
#endif //DMETAOPDBG	
	if(count < 1) return 0;

debug_metaop(2,("_GetDiscMeta Enter %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,disc_list->pt_loc,count,buf);
	if(ndStatus != NDAS_OK) return 0;
#ifdef DMETAOPDBG
	phead = (PON_DISC_ENTRY)buf;
debug_metaop(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DMETAOPDBG
debug_metaop(2,("---> _GetDiscMeta Leave %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	return 1;	
}

int GetDiscMetaMirror(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int startsector = disc_list->pt_loc + DISC_META_MIRROR_START;
#ifdef DMETAOPDBG
	PON_DISC_ENTRY phead;
#endif //DMETAOPDBG
	if(count < 1) return 0;

debug_metaop(2,("_GetDiscMetaMirror Enter %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_READ,startsector,count,buf);
	if(ndStatus != NDAS_OK) return 0;

#ifdef DMETAOPDBG
	phead = (PON_DISC_ENTRY)buf;
debug_metaop(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DMETAOPDBG

debug_metaop(2,("---> _GetDiscMetaMirror Leave %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	return 1;	
}

int SetDiscMeta(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DMETAOPDBG
	PON_DISC_ENTRY phead;
#endif //DMETAOPDBG	
	if(count < 1) return 0;

debug_metaop(2,("_SetDiscMeta Enter %p : pt_loc %d\n", sdev, disc_list->pt_loc));
#ifdef DMETAOPDBG
	phead = (PON_DISC_ENTRY)buf;
debug_metaop(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DMETAOPDBG


	debug_metaop(2,("_SetDiscMeta %p Loc %d\n",sdev, disc_list->pt_loc));

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_WRITE,disc_list->pt_loc,count,buf);
	if(ndStatus != NDAS_OK) return 0;

debug_metaop(2,("---> _SetDiscMeta Leave %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	return 1;	
}

int SetDiscMetaMirror(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int startsector = disc_list->pt_loc + DISC_META_MIRROR_START;
#ifdef DMETAOPDBG
	PON_DISC_ENTRY phead;
#endif //DMETAOPDBG
debug_metaop(2,("_SetDiscMetaMirror Enter %p : pt_loc %d\n", sdev, disc_list->pt_loc));
#ifdef DMETAOPDBG
	phead = (PON_DISC_ENTRY)buf;
debug_metaop(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DMETAOPDBG

	if(count < 1) return 0;
	debug_metaop(2,("_SetDiskMetaMirror %p Loc %d\n", sdev, disc_list->pt_loc));

	ndStatus = RawDiskSecureRWOp(sdev, ND_CMD_WRITE,startsector,count,buf);

	if(ndStatus != NDAS_OK) return 0;
debug_metaop(2,("---> _SetDiscMetaMirror Leave %p : pt_loc %d\n", sdev, disc_list->pt_loc));
	return 1;	
}

int
RawDiskThreadOp( 
	logunit_t *	sdev,
	int 				Command,
	unsigned long long 	start_sector,
	int					count,
	unsigned char *	 	buff
	)
{

#ifdef XPLAT_ASYNC_IO
    struct udev_request_block *tir = NULL;
    struct juke_io_blocking_done_arg darg;
#else
    udev_t * udev;
#endif
    ndas_error_t err;
    int ret;
#ifdef XPLAT_ASYNC_IO
    ndas_io_request *ioreq;

    tir = tir_alloc(Command, NULL);
    tir->func = NULL;
    tir->arg = 0;

    ioreq = tir->req;
    ioreq->buf = (xint8 *)buff;
    ioreq->start_sec = start_sector;;
    ioreq->num_sec = count;
    ioreq->done = juke_io_blocking_done;
    ioreq->done_arg = &darg;

    darg.done_sema = sal_semaphore_create("ndas_io_done", 1, 0);  
    sdev->ops->io(sdev, tir, FALSE);

    do {
        ret = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
    } while (ret == SAL_SYNC_INTERRUPTED);
    
    sal_semaphore_give(darg.done_sema);
    sal_semaphore_destroy(darg.done_sema);
    debug_metaop(5, ("ed err=%d", darg.err));
    err = darg.err;
#else
    udev = SDEV2UDEV(sdev);
    sal_assert(udev);
	
    if (udev->conn.writeshare_mode) {
        err = nwl_take(udev, 2000);
        if (!NDAS_SUCCESS(err)) 
            goto io_out;
    }    
    err = conn_rw(&udev->conn, Command,
        start_sector, count, buff);


io_out:
    if (udev->conn.writeshare_mode) {
        nwl_give(udev);
    }	
 #endif   
    if (!NDAS_SUCCESS(err)) {
        debug_metaop(1, ("conn_read_dib failed: %d", err));
        return err;
    }
    
    return NDAS_OK;
}
#endif //#ifdef XPLAT_JUKEBOX

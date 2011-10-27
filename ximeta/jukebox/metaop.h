#ifndef _NDAS_META_H_
#define _NDAS_META_H_


#ifdef XPLAT_JUKEBOX

struct juke_io_blocking_done_arg {
    sal_semaphore done_sema;
    ndas_error_t err;
};

void juke_io_blocking_done(int slot, ndas_error_t err, void* arg);

int GetDiskMeta(logunit_t * sdev, unsigned char * buf, int count);

int GetDiskMetaMirror(logunit_t * sdev, unsigned char * buf, int count);

int GetDiskLogHead(logunit_t * sdev, unsigned char * buf, int count);

int GetDiskLogData(logunit_t * sdev, unsigned char * buf, int index, int count);

int SetDiskMeta(logunit_t * sdev, unsigned char * buf, int count);

int SetDiskMetaMirror(logunit_t * sdev, unsigned char * buf, int count);

int SetDiskLogHead(logunit_t * sdev, unsigned char * buf, int count);

int SetDiskLogData(logunit_t * sdev, unsigned char * buf, int index, int count);

int GetDiscMeta(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count);

int GetDiscMetaMirror(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count);

int SetDiscMeta(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count);

int SetDiscMetaMirror(logunit_t * sdev, PDISC_LIST disc_list, unsigned char * buf, int count);

int RawDiskThreadOp( 
	logunit_t *	sdev,
	int 				Command,
	unsigned long long 	start_sector,
	int					count,
	unsigned char *	 	buff
	);

#endif //#ifdef XPLAT_JUKEBOX

#endif //_NDAS_META_H_


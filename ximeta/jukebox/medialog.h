#ifndef _NDAS_LOG_H_
#define _NDAS_LOG_H_

#ifdef XPLAT_JUKEBOX
int getHashVal(int hostid);

int getLogIndex(PON_DISK_LOG logdata,int hostid);

int getLogRefCount(PON_DISK_LOG logdata, int index);

void setLogRefCount(PON_DISK_LOG logdata, int index, unsigned int value);

int setLogIndex(PON_DISK_LOG logdata, int hostid);

void dumpHostRef(PON_DISK_LOG logdata);

void dumpDiscHistory(PON_DISK_LOG logdata);

void dumpDisKHistory(PON_DISK_LOG_HEADER loghead);

int _GetDiscRefCount(logunit_t * sdev, unsigned int selectedDisc);

// _AddRefCount and _RemoveRefCount must be done after Error checking routine
//	_CheckCurrentDisk
//	_CheckCurrentDisc
//	_DoDelayedOperation

int _AddRefCount(
		logunit_t * sdev,
		unsigned int selectedDisc,
		unsigned int hostid);

int _RemoveRefCount(
		logunit_t * sdev,
		unsigned int selectedDisc,
		unsigned int hostid);

int _WriteAndLogStart(
		logunit_t * sdev, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc, 
		unsigned int sector_count,
		unsigned int encrypt);

int _WriteAndLogEnd(
		logunit_t * sdev, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc);

int _DeleteAndLog(
		logunit_t * sdev, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc);

int _CheckUpdateDisk(logunit_t	*sdev,  unsigned int update, unsigned int hostid);

int _CheckUpdateSelectedDisk(
	logunit_t	*sdev,  
	unsigned int update, 
	unsigned int hostid, 
	unsigned int selected_disc);

/*
must call GetDiskMeta   GetDiscMeta before call this function 
*/
int _InvalidateDisc(
	logunit_t	*sdev,  
	unsigned char * buf,
	unsigned char * buf2, 
	unsigned int hostid);

int _CheckWriteFail(
	logunit_t * sdev, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	);

int _CheckDeleteFail(
	logunit_t * sdev, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	);

int _CheckHostOwner(
	logunit_t * sdev, 
	unsigned int		selectedDisc,
	unsigned int 		hostid
	);



int _CheckValidateCount(
		logunit_t * sdev, 
		unsigned int selected_disc
		);
#endif //#ifdef XPLAT_JUKEBOX

#endif //#ifndef _NDAS_LOG_H_


#ifndef _NDAS_METAINFO_H_
#define _NDAS_METAINFO_H_
#ifdef XPLAT_JUKEBOX
/********************************************************************

	Bitmap Operation

*********************************************************************/
#ifndef XPLAT_EMBEDDED
static inline int test_bit(int nr, const void * addr)
{
    return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}	

static inline void set_big(int nr, volatile void *addr)
{
    ((unsigned char *) addr)[nr >> 3] |= (1U << (nr & 7));
}

static inline void clear_bit(int nr, volatile void *addr)
{
    ((unsigned char *) addr)[nr >> 3] &= ~(1U << (nr & 7));
}

static inline void change_bit(int nr, volatile void *addr)
{
    ((unsigned char *) addr)[nr >> 3] ^= (1U << (nr & 7));
}

static inline int test_and_set_bit(int nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval | mask;
	return oldval & mask;
}

static inline int test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval & ~mask;
	return oldval & mask;
}

static inline int test_and_change_bit(int nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;
	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval ^ mask;
	return oldval & mask;

}
#endif//#ifndef XPLAT_EMBEDDED
int findFreeMapCount(unsigned int bitmap_count, PBITMAP bitmap);
int findDirtyMapCount(unsigned int bitmap_count, PBITMAP bitmap);
void setDiscAddr_diskFreeMap(unsigned int bitmap_count, unsigned int request_alloc, PBITMAP diskfreemap, PON_DISC_ENTRY entry);
void setDiscAddrtoLogData(PON_DISC_ENTRY entry, PON_DISK_LOG logdata);
void freeClusterMap (unsigned int index , PBITMAP DiscMap);
void setClusterMap (unsigned int index, PBITMAP DiscMap);
void setClusterMapFromEntry(PBITMAP clustermap, PON_DISC_ENTRY entry);
void freeClusterMapFromEntry(PBITMAP clustermap, PON_DISC_ENTRY entry);
void setAllClusterMapFromEntry(PBITMAP freeclustermap, PBITMAP dirtyclustermap, PON_DISC_ENTRY entry);
void freeAllClusterMapFromEntry(PBITMAP freeclustermap, PBITMAP dirtyclustermap, PON_DISC_ENTRY entry);
void freeDiscMapbyLogData(PBITMAP freeMap, PBITMAP dirtyMap, PON_DISK_LOG logdata);
void freeDiscInfoMap (unsigned int index , PBITMAP DiscMap);
void setDiscInfoMap (unsigned int index, PBITMAP DiscMap);

void SetDiskMetaInformation(logunit_t * sdev, PON_DISK_META buf, unsigned int update);
void AllocAddrEntities(struct media_addrmap * amap, PON_DISC_ENTRY entry);
void InitDiscEntry(
	logunit_t * sdev, 
	unsigned int cluster_size, 
	unsigned int index, 
	unsigned int loc, 
	PON_DISC_ENTRY buf);
void SetDiscEntryInformation (logunit_t * sdev, PON_DISC_ENTRY buf, unsigned int update);
int SetDiscAddr(logunit_t *sdev, struct media_addrmap * amap, int disc);
#endif //#ifdef XPLAT_JUKEBOX
#endif //_NDAS_METAINFO_H_


/*
 *  linux/fs/fat/misc.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *  22/11/2000 - Fixed fat_date_unix2dos for dates earlier than 01/01/1980
 *		 and date_dos2unix for date==0 by Igor Zhbanov(bsg@uniyar.ac.ru)
 */
#include <linux/version.h>
#include <linux/module.h> 


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

#include <linux/fs.h>
//#include <linux/msdos_fs.h>
#include "ndasfat_fs.h"
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/locks.h>

#if 0 
#  define PRINTK(x)	printk x
#else
#  define PRINTK(x)
#endif
#define Printk(x)	printk x

#ifdef DEBUG

extern int	dbg_level_ndasfat;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_ndasfat ) {							\
        printk("FAT|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

/* Well-known binary file extensions - of course there are many more */

static char ascii_extensions[] =
  "TXT" "ME " "HTM" "1ST" "LOG" "   " 	/* text files */
  "C  " "H  " "CPP" "LIS" "PAS" "FOR"  /* programming languages */
  "F  " "MAK" "INC" "BAS" 		/* programming languages */
  "BAT" "SH "				/* program code :) */
  "INI"					/* config files */
  "PBM" "PGM" "DXF"			/* graphics */
  "TEX";				/* TeX */

/*
 * fat_fs_panic reports a severe file system problem and sets the file system
 * read-only. The file system can be made writable again by remounting it.
 */

void fat_fs_panic(struct super_block *s,const char *msg)
{
	int not_ro;

	not_ro = !(s->s_flags & MS_RDONLY);
	if (not_ro) s->s_flags |= MS_RDONLY;
	printk("Filesystem panic (dev %s).\n  %s\n", kdevname(s->s_dev), msg);
	if (not_ro)
		printk("  File system has been set read-only\n");
}


/*
 * fat_is_binary selects optional text conversion based on the conversion mode
 * and the extension part of the file name.
 */

int fat_is_binary(char conversion,char *extension)
{
	char *walk;

	switch (conversion) {
		case 'b':
			return 1;
		case 't':
			return 0;
		case 'a':
			for (walk = ascii_extensions; *walk; walk += 3)
				if (!strncmp(extension,walk,3)) return 0;
			return 1;	/* default binary conversion */
		default:
			printk("Invalid conversion mode - defaulting to "
			    "binary.\n");
			return 1;
	}
}

void lock_fat(struct super_block *sb)
{
	down(&(MSDOS_SB(sb)->fat_lock));
}

void unlock_fat(struct super_block *sb)
{
	up(&(MSDOS_SB(sb)->fat_lock));
}

#ifdef NDAS_CRYPTO
#define MAX_FREE_ALLOC 64 
void fat_cluster_lock(struct super_block *sb)
{
	int	err = -1;
	int 	count = 0;
	ndas_lock lock;
        
	down(&MSDOS_SB(sb)->cluster_lock);
//	printk("cluster_lock aquire\n");
	count = 0;
	while(1) { 
		lock.lock_type = 1;
		err = ioctl_by_bdev( sb->s_bdev,  IOCTL_NDAS_LOCK, (unsigned long)&lock );
		if(err != 0) {
			printk( "lock retry: lock err = %d, count = %d, lock.lock_status = %d\n", err, count, lock.lock_status );
			if (count > 10) break;
			count ++;
			continue;
		}
		break;
	}
	up(&MSDOS_SB(sb)->cluster_lock);
	return;
}

void fat_cluster_unlock(struct super_block *sb)
{
	int	err = -1;
	ndas_lock lock;
    
	down(&MSDOS_SB(sb)->cluster_lock);
//	printk("cluster_lock release\n");
	lock.lock_type = 0;
	err = ioctl_by_bdev( sb->s_bdev,  IOCTL_NDAS_LOCK, (unsigned long)&lock );
	if(err != 0) {
		printk( "lock err = %d, s_lock.lock_status = %d\n", err, lock.lock_status );
	}
	up(&MSDOS_SB(sb)->cluster_lock);
	return;
}

static void dump_dede(struct msdos_dir_entry *de)
{
	int i;
	unsigned char *p = (unsigned char *) de;
	printk("[");

	for (i = 0; i < 32; i++, p++) {
		printk("%02x ", *p);
	}
	printk("]\n");
}

void fat_buffer_sync(struct super_block *sb, struct buffer_head *bh)
{
#if 0
		if (bh->b_blocknr == 20504)
		{
			struct msdos_dir_entry *temp_de;
			int i;

			dbg_call(1, "fat_buffer_sync: location = %ld\n", bh->b_blocknr);

			temp_de = (struct msdos_dir_entry *) bh->b_data;

			for(i=0;i<16;i++) {
				printk("[%s,%d,%d]", temp_de->name, CF_LE_W(temp_de->start), CF_LE_W(temp_de->starthi));
//				dump_dede(temp_de);
				temp_de ++;
			}
			printk("\n");
		}
#endif
	if (!bh) return;

	if(buffer_uptodate(bh)) {
		wait_on_buffer(bh);
		ll_rw_block(WRITE, 1, &bh);
		wait_on_buffer(bh);
	}
	else {
		printk("FAT error: buffer is not uptodate\n");
	}
}

struct buffer_head *fat_bread_direct(struct super_block *sb, int block)
{
	struct buffer_head *bh;

	bh = getblk(sb->s_dev, block, sb->s_blocksize);
	if (!bh) return NULL;

	SetPageReferenced(bh->b_page);

	if(buffer_uptodate(bh) && buffer_dirty(bh)) {
		printk("dirty!!dirty!! location = %ld, size = %d\n", bh->b_blocknr, bh->b_size);
//		return bh;
	}

	wait_on_buffer(bh);
	mark_buffer_uptodate(bh, 0);
	ll_rw_block(READ, 1, &bh);
	wait_on_buffer(bh);

	if(buffer_uptodate(bh)) {
#if 0
		if (bh->b_blocknr == 20504) 
		{
			struct msdos_dir_entry *temp_de;
			int i;

			dbg_call(1, "location = %ld\n", bh->b_blocknr);

			temp_de = (struct msdos_dir_entry *) bh->b_data;

			for(i=0;i<16;i++) {
				printk("[%s,%d,%d]", temp_de->name, CF_LE_W(temp_de->start), CF_LE_W(temp_de->starthi));
//				dump_dede(temp_de);
				temp_de ++;
			}
			printk("\n");
		}
#endif
		return bh;
	}

	printk("READ:buffer is not uptodate\n");

	brelse(bh);
	return NULL;
	
}

static void fat_allocate_free_cluster_list(struct super_block *sb)
{
	struct buffer_head *bh = NULL, *bh2 = NULL, *c_bh = NULL, *c_bh2 = NULL;
	unsigned char *p_first, *p_last;
	int copy, first, second, last, next, b, s, prev_b, copy_b;
	int count, nr, limit;
	struct cluster_node *c_node;
	int block_last_cluster, new_block;
	int eof_value;

	limit = MSDOS_SB(sb)->clusters;
	nr = limit; /* to keep GCC happy */
	eof_value = EOF_FAT(sb);

//	down(&MSDOS_SB(sb)->fat_free_cluster_list_lock);
//	dbg_call(1, "fats = %d, block_size = %ld, block_size_bits = %d, prev_free = %d, free_clusters = %d, free_cluster_alloc_count = %d\n", MSDOS_SB(sb)->fats, sb->s_blocksize, sb->s_blocksize_bits, MSDOS_SB(sb)->prev_free, MSDOS_SB(sb)->free_clusters, MSDOS_SB(sb)->fat_free_cluster_alloc_count);

	prev_b = -1;
	count = 0;
	new_block = 1;
	block_last_cluster = 0;
	while(block_last_cluster == 0  || MSDOS_SB(sb)->fat_free_cluster_alloc_count < MAX_FREE_ALLOC) {
		nr = ((count + MSDOS_SB(sb)->prev_free) % limit) + 2;

		if (MSDOS_SB(sb)->fat_bits == 32) {
			first = last = nr*4;
			second = (nr+1)*4;
		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			first = last = nr*2;
			second = (nr+1)*2;
		} else {
			first = nr*3/2;
			second = (nr+1)*3/2;
			last = first+1;
		}

		b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits);
		s = MSDOS_SB(sb)->fat_start + (second >> sb->s_blocksize_bits);

		if (b != s) {
			block_last_cluster = 1;
		}
		else {
			block_last_cluster = 0;
		}

		new_block = ( b != prev_b );

		if (new_block) {
			if (!(bh = fat_bread_direct(sb, b))) {
				printk("bread in fat_access failed\n");
				break;
			}
		}

		if ((first >> sb->s_blocksize_bits) == (last >> sb->s_blocksize_bits)) {
			bh2 = bh;
		} else {
			if ( new_block ) {
				if (!(bh2 = fat_bread_direct(sb, b+1))) {
					fat_brelse(sb, bh);
					printk("2nd bread in fat_access failed\n");
					break;
				}
			}
		}

//		dbg_call(1, "count = %d, block_last_cluster = %d, new_block = %d, bh->b_blocknr = %ld, bh2->b_blocknr = %ld\n", count, block_last_cluster, new_block, bh->b_blocknr, bh2->b_blocknr);

		if (MSDOS_SB(sb)->fat_bits == 32) {
			p_first = p_last = NULL; /* GCC needs that stuff */
			next = CF_LE_L(((__u32 *) bh->b_data)[(first &
			    (sb->s_blocksize - 1)) >> 2]);
			/* Fscking Microsoft marketing department. Their "32" is 28. */
			next &= 0xfffffff;
			if (next >= 0xffffff7) next = -1;
//			dbg_call( 1, "fat_bread: 0x%x, nr=0x%x, first=0x%x, next=0x%x\n", b, nr, first, next);

		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			p_first = p_last = NULL; /* GCC needs that stuff */
			next = CF_LE_W(((__u16 *) bh->b_data)[(first &
			    (sb->s_blocksize - 1)) >> 1]);
			if (next >= 0xfff7) next = -1;
		} else {
			p_first = &((__u8 *)bh->b_data)[first & (sb->s_blocksize - 1)];
			p_last = &((__u8 *)bh2->b_data)[(first + 1) & (sb->s_blocksize - 1)];
			if (nr & 1) next = ((*p_first >> 4) | (*p_last << 4)) & 0xfff;
			else next = (*p_first+(*p_last << 8)) & 0xfff;
			if (next >= 0xff7) next = -1;
		}

		if(next == 0) {
			c_node = (struct cluster_node *) kmalloc(sizeof(struct cluster_node), GFP_KERNEL);
			memset(c_node, 0, sizeof(struct cluster_node));
			c_node->cluster_nr = nr;
			list_add_tail(&c_node->node, &MSDOS_SB(sb)->fat_free_cluster_list);
			MSDOS_SB(sb)->fat_free_cluster_alloc_count ++;

			MSDOS_SB(sb)->prev_free = (count + MSDOS_SB(sb)->prev_free + 1) % limit;
			if (MSDOS_SB(sb)->free_clusters != -1)
				MSDOS_SB(sb)->free_clusters--;
			else
				printk("can't allocate cluster: no free space\n");
//			printk("[%d]", nr);

			count = -1;
//			dbg_call(1, "%dth insert queue = %p(%d)\n", MSDOS_SB(sb)->fat_free_cluster_alloc_count, c_node, nr);

			if (MSDOS_SB(sb)->fat_bits == 32) {
				((__u32 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 2]
					= CT_LE_L(eof_value);
			} else if (MSDOS_SB(sb)->fat_bits == 16) {
				((__u16 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 1]
					= CT_LE_W(eof_value);
			} else {
				if (nr & 1) {
					*p_first = (*p_first & 0xf) | (eof_value << 4);
					*p_last = eof_value >> 4;
				}
				else {
					*p_first = eof_value & 0xff;
					*p_last = (*p_last & 0xf0) | (eof_value >> 8);
				}
				fat_mark_buffer_dirty(sb, bh2);
			}
			fat_mark_buffer_dirty(sb, bh);
		}

		if ( block_last_cluster ) {
			for (copy = 1; copy < MSDOS_SB(sb)->fats; copy++) {
				copy_b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits)
					+ MSDOS_SB(sb)->fat_length * copy;

				if(buffer_dirty(bh)) {
					if (!(c_bh = fat_bread_direct(sb, copy_b)))
						break;
	
					if (bh != bh2) {
						if (!(c_bh2 = fat_bread_direct(sb, copy_b+1))) {
							fat_brelse(sb, c_bh);
							break;
						}
						memcpy(c_bh2->b_data, bh2->b_data, sb->s_blocksize);
						fat_mark_buffer_dirty(sb, c_bh2);
						fat_buffer_sync(sb, c_bh2);
						fat_brelse(sb, c_bh2);
					}
	
					memcpy(c_bh->b_data, bh->b_data, sb->s_blocksize);
					fat_mark_buffer_dirty(sb, c_bh);
					fat_buffer_sync(sb, c_bh);
					fat_brelse(sb, c_bh);
//					dbg_call(1, "copy_b = %d, cbh->b_blocknr = %ld, cbh2->b_blocknr = %ld\n", copy_b, c_bh->b_blocknr, c_bh2->b_blocknr);
				}
			}
/*
			{
				int i;
				
				for(i=0;i<bh->b_size;i++) {
					printk("%02X:", (unsigned char)bh->b_data[i]);
				}
				printk("\n");
			}
*/
			fat_buffer_sync(sb, bh);
			fat_brelse(sb, bh);
			if (bh != bh2) {
				fat_buffer_sync(sb, bh2);
				fat_brelse(sb, bh2);
			}
		}

		prev_b = b;
		count ++;
	}

//	up(&MSDOS_SB(sb)->fat_free_cluster_list_lock);
	dbg_call(1, "ended\n");

	return;
}

void fat_deallocate_free_cluster_list(struct super_block *sb)
{
	struct buffer_head *bh, *bh2, *c_bh, *c_bh2;
	unsigned char *p_first, *p_last;
	int copy, first, last, next, b;
	int nr, limit;
	struct list_head *i;
	struct cluster_node *node;
	int zero_value = 0;

	limit = MSDOS_SB(sb)->clusters;

//	down(&MSDOS_SB(sb)->fat_free_cluster_list_lock);
	dbg_call(1, "prev_free = %d, free_clusters = %d, free_cluster_alloc_count = %d\n", MSDOS_SB(sb)->prev_free, MSDOS_SB(sb)->free_clusters, MSDOS_SB(sb)->fat_free_cluster_alloc_count);

	i = MSDOS_SB(sb)->fat_free_cluster_list.next;
	node = list_entry(i, struct cluster_node, node);
//	MSDOS_SB(sb)->prev_free = node->cluster_nr - 2; // prev_free point smallest free cluster

	while(!list_empty(&MSDOS_SB(sb)->fat_free_cluster_list)) {
		i = MSDOS_SB(sb)->fat_free_cluster_list.next;
		node = list_entry(i, struct cluster_node, node);
		nr = node->cluster_nr;
		dbg_call(1, "delete queue = %d\n", nr);
		list_del(i);
		kfree(node);

		if (MSDOS_SB(sb)->fat_bits == 32) {
			first = last = nr*4;
		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			first = last = nr*2;
		} else {
			first = nr*3/2;
			last = first+1;
		}
	
		b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits);

		if (!(bh = fat_bread(sb, b))) {
			printk("bread in fat_access failed\n");
			break;
		}

		if ((first >> sb->s_blocksize_bits) == (last >> sb->s_blocksize_bits)) {
			bh2 = bh;
		} else {
			if (!(bh2 = fat_bread(sb, b+1))) {
				fat_brelse(sb, bh);
				printk("2nd bread in fat_access failed\n");
				break;;
			}
		}
		if (MSDOS_SB(sb)->fat_bits == 32) {
			p_first = p_last = NULL;  /* GCC needs that stuff */
			next = CF_LE_L(((__u32 *) bh->b_data)[(first &
			    (sb->s_blocksize - 1)) >> 2]);
			/* Fscking Microsoft marketing department. Their "32" is 28. */
			next &= 0xfffffff;
			if (next >= 0xffffff7) next = -1;
			PRINTK(("fat_bread: 0x%x, nr=0x%x, first=0x%x, next=0x%x\n", b, nr, first, next));
	
		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			p_first = p_last = NULL; /* GCC needs that stuff */
			next = CF_LE_W(((__u16 *) bh->b_data)[(first &
			    (sb->s_blocksize - 1)) >> 1]);
			if (next >= 0xfff7) next = -1;
		} else {
			p_first = &((__u8 *)bh->b_data)[first & (sb->s_blocksize - 1)];
			p_last = &((__u8 *)bh2->b_data)[(first + 1) & (sb->s_blocksize - 1)];
			if (nr & 1) next = ((*p_first >> 4) | (*p_last << 4)) & 0xfff;
			else next = (*p_first+(*p_last << 8)) & 0xfff;
			if (next >= 0xff7) next = -1;
		}


		if (MSDOS_SB(sb)->fat_bits == 32) {
			((__u32 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 2]
				= CT_LE_L(zero_value);
		} else if (MSDOS_SB(sb)->fat_bits == 16) {
			((__u16 *)bh->b_data)[(first & (sb->s_blocksize - 1)) >> 1]
				= CT_LE_W(zero_value);
		} else {
			if (nr & 1) {
				*p_first = (*p_first & 0xf) | (zero_value << 4);
				*p_last = zero_value >> 4;
			}
			else {
				*p_first = zero_value & 0xff;
				*p_last = (*p_last & 0xf0) | (zero_value >> 8);
			}
			fat_mark_buffer_dirty(sb, bh2);
		}
		fat_mark_buffer_dirty(sb, bh);
		for (copy = 1; copy < MSDOS_SB(sb)->fats; copy++) {
			b = MSDOS_SB(sb)->fat_start + (first >> sb->s_blocksize_bits)
				+ MSDOS_SB(sb)->fat_length * copy;
			if (!(c_bh = fat_bread(sb, b)))
				break;
			if (bh != bh2) {
				if (!(c_bh2 = fat_bread(sb, b+1))) {
					fat_brelse(sb, c_bh);
					break;
				}
				memcpy(c_bh2->b_data, bh2->b_data, sb->s_blocksize);
				fat_mark_buffer_dirty(sb, c_bh2);
				fat_brelse(sb, c_bh2);
			}
			memcpy(c_bh->b_data, bh->b_data, sb->s_blocksize);
			fat_mark_buffer_dirty(sb, c_bh);
			fat_brelse(sb, c_bh);
		}

		fat_brelse(sb, bh);
		if (bh != bh2)
			fat_brelse(sb, bh2);

		if (MSDOS_SB(sb)->free_clusters < limit)
			MSDOS_SB(sb)->free_clusters++;
	}

//	up(&MSDOS_SB(sb)->fat_free_cluster_list_lock);

	return;
}

int fat_get_free_cluster(struct super_block *sb)
{
	struct list_head *i;
	struct cluster_node *c_node;
	int nr = 0;

//	down(&MSDOS_SB(sb)->fat_free_cluster_list_lock);

	i = MSDOS_SB(sb)->fat_free_cluster_list.next;
	c_node = list_entry(i, struct cluster_node, node);
	if(c_node) {
		nr = c_node->cluster_nr;
		list_del(&c_node->node);
	
		MSDOS_SB(sb)->fat_free_cluster_alloc_count --;

		dbg_call( 8, "fat_free_cluster_alloc_count = %d, delete queue = %p(%d)\n", 
					 MSDOS_SB(sb)->fat_free_cluster_alloc_count, c_node, nr );
		kfree(c_node);
	}
//	up(&MSDOS_SB(sb)->fat_free_cluster_list_lock);

	return  nr;
}

struct buffer_head * fat_clusters_read(struct super_block *sb)
{
	struct buffer_head *bh;
	struct fat_boot_fsinfo *fsinfo;

	bh = fat_bread_direct(sb, MSDOS_SB(sb)->fsinfo_sector);
	if (bh == NULL) {
		printk("FAT bread failed in fat_clusters_read\n");
		return NULL;
	}

	fsinfo = (struct fat_boot_fsinfo *)bh->b_data;
	/* Sanity check */
	if (!IS_FSINFO(fsinfo)) {
		printk("FAT: Did not find valid FSINFO signature.\n"
		       "Found signature1 0x%x signature2 0x%x sector=%ld.\n",
		       CF_LE_L(fsinfo->signature1), CF_LE_L(fsinfo->signature2),
		       MSDOS_SB(sb)->fsinfo_sector);
		dbg_call(1, "free_cluster = %d, prev_free = %d\n", MSDOS_SB(sb)->free_clusters, MSDOS_SB(sb)->prev_free);
		return NULL;
	}
	MSDOS_SB(sb)->free_clusters = CF_LE_L(fsinfo->free_clusters);
	MSDOS_SB(sb)->prev_free = CF_LE_L(fsinfo->next_cluster);

	dbg_call(1, "free_cluster = %d, prev_free = %d\n", MSDOS_SB(sb)->free_clusters, MSDOS_SB(sb)->prev_free);

	return bh;
}
#endif

/* Flushes the number of free clusters on FAT32 */
/* XXX: Need to write one per FSINFO block.  Currently only writes 1 */

#ifdef NDAS_CRYPTO
void fat_clusters_flush(struct super_block *sb, struct buffer_head *bh)
#else
void fat_clusters_flush(struct super_block *sb)
#endif
{
#ifdef NDAS_CRYPTO
#else
	struct buffer_head *bh;
#endif
	struct fat_boot_fsinfo *fsinfo;

#ifdef NDAS_CRYPTO
#else
	bh = fat_bread(sb, MSDOS_SB(sb)->fsinfo_sector);
	if (bh == NULL) {
		printk("FAT bread failed in fat_clusters_flush\n");
		return;
	}
#endif

	fsinfo = (struct fat_boot_fsinfo *)bh->b_data;
	/* Sanity check */
	if (!IS_FSINFO(fsinfo)) {
		printk("FAT: Did not find valid FSINFO signature.\n"
		       "Found signature1 0x%x signature2 0x%x sector=%ld.\n",
		       CF_LE_L(fsinfo->signature1), CF_LE_L(fsinfo->signature2),
		       MSDOS_SB(sb)->fsinfo_sector);
		return;
	}
	fsinfo->free_clusters = CF_LE_L(MSDOS_SB(sb)->free_clusters);
	fsinfo->next_cluster = CF_LE_L(MSDOS_SB(sb)->prev_free);
	fat_mark_buffer_dirty(sb, bh);
#ifdef NDAS_CRYPTO
	fat_buffer_sync(sb,bh);
#endif
	fat_brelse(sb, bh);
	dbg_call(1, "free_cluster = %d, prev_free = %d\n", MSDOS_SB(sb)->free_clusters, MSDOS_SB(sb)->prev_free);
}

/*
 * fat_add_cluster tries to allocate a new cluster and adds it to the
 * file represented by inode.
 */
int fat_add_cluster(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	int nr, last, curr, file_cluster;
#ifdef NDAS_CRYPTO
	struct buffer_head *bh = NULL;
#else
	int count, limit;
#endif
	int cluster_size = MSDOS_SB(sb)->cluster_size;
	int res = -ENOSPC;
	
	lock_fat(sb);

	if (MSDOS_SB(sb)->free_clusters == 0) {
		unlock_fat(sb);
		return res;
	}

#ifdef NDAS_CRYPTO
	if(list_empty(&MSDOS_SB(sb)->fat_free_cluster_list)) {
		if (MSDOS_SB(sb)->fat_bits == 32) {
			bh = fat_clusters_read(sb);
			if (!bh) {
				printk(" can't read clusters info.\n");
				unlock_fat(sb);
				return -EIO;
			}
		}

		fat_allocate_free_cluster_list(sb);
		if (MSDOS_SB(sb)->fat_bits == 32)
			fat_clusters_flush(sb,bh);
	}

	nr = fat_get_free_cluster(sb);
	dbg_call(1, "[%d]\n", nr);
//	fat_access(sb, nr, EOF_FAT(sb));

#else
	limit = MSDOS_SB(sb)->clusters;
	nr = limit; /* to keep GCC happy */
	for (count = 0; count < limit; count++) {
		nr = ((count + MSDOS_SB(sb)->prev_free) % limit) + 2;
		if (fat_access(sb, nr, -1) == 0)
			break;
	}
	if (count >= limit) {
		MSDOS_SB(sb)->free_clusters = 0;
		unlock_fat(sb);
		return res;
	}
	
	MSDOS_SB(sb)->prev_free = (count + MSDOS_SB(sb)->prev_free + 1) % limit;
	fat_access(sb, nr, EOF_FAT(sb));
	if (MSDOS_SB(sb)->free_clusters != -1)
		MSDOS_SB(sb)->free_clusters--;

	if (MSDOS_SB(sb)->fat_bits == 32)
		fat_clusters_flush(sb);
#endif

	unlock_fat(sb);
	
	/* We must locate the last cluster of the file to add this
	   new one (nr) to the end of the link list (the FAT).
	   
	   Here file_cluster will be the number of the last cluster of the
	   file (before we add nr).
	   
	   last is the corresponding cluster number on the disk. We will
	   use last to plug the nr cluster. We will use file_cluster to
	   update the cache.
	*/
	last = file_cluster = 0;
	if ((curr = MSDOS_I(inode)->i_start) != 0) {
		fat_cache_lookup(inode, INT_MAX, &last, &curr);
		file_cluster = last;
		while (curr && curr != -1){
			file_cluster++;
			if (!(curr = fat_access(sb, last = curr,-1))) {
				fat_fs_panic(sb, "File without EOF");
				return res;
			}
		}
	}
	if (last) {
		fat_access(sb, last, nr);
		fat_cache_add(inode, file_cluster, nr);
	} else {
		MSDOS_I(inode)->i_start = nr;
		MSDOS_I(inode)->i_logstart = nr;
		mark_inode_dirty(inode);
	}
	if (file_cluster
	    != inode->i_blocks / cluster_size / (sb->s_blocksize / 512)) {
		printk ("file_cluster badly computed!!! %d <> %ld\n",
			file_cluster,
			inode->i_blocks / cluster_size / (sb->s_blocksize / 512));
		fat_cache_inval_inode(inode);
	}
	inode->i_blocks += (1 << MSDOS_SB(sb)->cluster_bits) / 512;

	return nr;
}

struct buffer_head *fat_extend_dir(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	int nr, sector, last_sector;
	struct buffer_head *bh, *res = NULL;
	int cluster_size = MSDOS_SB(sb)->cluster_size;

	if (MSDOS_SB(sb)->fat_bits != 32) {
		if (inode->i_ino == MSDOS_ROOT_INO)
			return res;
	}

	nr = fat_add_cluster(inode);
	dbg_call(1, "nr = %d\n", nr);
	if (nr < 0)
		return res;
	
	sector = MSDOS_SB(sb)->data_start + (nr - 2) * cluster_size;
	last_sector = sector + cluster_size;
	if (MSDOS_SB(sb)->ndas_cvf_format && MSDOS_SB(sb)->ndas_cvf_format->zero_out_cluster)
		MSDOS_SB(sb)->ndas_cvf_format->zero_out_cluster(inode, nr);
	else {
		for ( ; sector < last_sector; sector++) {
#ifdef DEBUG
//			printk("zeroing sector %d\n", sector);
#endif
			if (!(bh = fat_getblk(sb, sector)))
				printk("getblk failed\n");
			else {
				memset(bh->b_data, 0, sb->s_blocksize);
				fat_set_uptodate(sb, bh, 1);
				fat_mark_buffer_dirty(sb, bh);
				if (!res)
					res = bh;
				else {
#ifdef NDAS_CRYPTO
					if (inode->i_ino == MSDOS_ROOT_INO) {
						fat_buffer_sync(sb, bh);
					}
#endif
					fat_brelse(sb, bh);
				}
			}
		}
	}
	if (inode->i_size & (sb->s_blocksize - 1)) {
		fat_fs_panic(sb, "Odd directory size");
		inode->i_size = (inode->i_size + sb->s_blocksize)
			& ~(sb->s_blocksize - 1);
	}
	inode->i_size += 1 << MSDOS_SB(sb)->cluster_bits;
	MSDOS_I(inode)->mmu_private += 1 << MSDOS_SB(sb)->cluster_bits;
	mark_inode_dirty(inode);

	return res;
}

/* Linear day numbers of the respective 1sts in non-leap years. */

static int day_n[] = { 0,31,59,90,120,151,181,212,243,273,304,334,0,0,0,0 };
		  /* JanFebMarApr May Jun Jul Aug Sep Oct Nov Dec */


extern struct timezone sys_tz;


/* Convert a MS-DOS time/date pair to a UNIX date (seconds since 1 1 70). */

int date_dos2unix(unsigned short time,unsigned short date)
{
	int month,year,secs;

	/* first subtract and mask after that... Otherwise, if
	   date == 0, bad things happen */
	month = ((date >> 5) - 1) & 15;
	year = date >> 9;
	secs = (time & 31)*2+60*((time >> 5) & 63)+(time >> 11)*3600+86400*
	    ((date & 31)-1+day_n[month]+(year/4)+year*365-((year & 3) == 0 &&
	    month < 2 ? 1 : 0)+3653);
			/* days since 1.1.70 plus 80's leap day */
	secs += sys_tz.tz_minuteswest*60;
	return secs;
}


/* Convert linear UNIX date to a MS-DOS time/date pair. */

void fat_date_unix2dos(int unix_date,unsigned short *time,
    unsigned short *date)
{
	int day,year,nl_day,month;

	unix_date -= sys_tz.tz_minuteswest*60;

	/* Jan 1 GMT 00:00:00 1980. But what about another time zone? */
	if (unix_date < 315532800)
		unix_date = 315532800;

	*time = (unix_date % 60)/2+(((unix_date/60) % 60) << 5)+
	    (((unix_date/3600) % 24) << 11);
	day = unix_date/86400-3652;
	year = day/365;
	if ((year+3)/4+365*year > day) year--;
	day -= (year+3)/4+365*year;
	if (day == 59 && !(year & 3)) {
		nl_day = day;
		month = 2;
	}
	else {
		nl_day = (year & 3) || day <= 59 ? day : day-1;
		for (month = 0; month < 12; month++)
			if (day_n[month] > nl_day) break;
	}
	*date = nl_day-day_n[month-1]+1+(month << 5)+(year << 9);
}


/* Returns the inode number of the directory entry at offset pos. If bh is
   non-NULL, it is brelse'd before. Pos is incremented. The buffer header is
   returned in bh.
   AV. Most often we do it item-by-item. Makes sense to optimize.
   AV. OK, there we go: if both bh and de are non-NULL we assume that we just
   AV. want the next entry (took one explicit de=NULL in vfat/namei.c).
   AV. It's done in fat_get_entry() (inlined), here the slow case lives.
   AV. Additionally, when we return -1 (i.e. reached the end of directory)
   AV. we make bh NULL. 
 */

int fat__get_entry(struct inode *dir, loff_t *pos,struct buffer_head **bh,
		   struct msdos_dir_entry **de, loff_t *i_pos)
{
	struct super_block *sb = dir->i_sb;
	struct ndasfat_sb_info *sbi = MSDOS_SB(sb);
	int sector;
	loff_t offset;

	while (1) {
		offset = *pos;
		PRINTK (("get_entry offset %d\n",offset));
		if (*bh) {
#ifdef NDAS_CRYPTO
			if (dir->i_ino == MSDOS_ROOT_INO) {
				fat_buffer_sync(sb, *bh);
			}
#endif
			fat_brelse(sb, *bh);
		}
		*bh = NULL;
		if ((sector = fat_bmap(dir,offset >> sb->s_blocksize_bits)) == -1)
			return -1;
		PRINTK (("get_entry sector %d %p\n",sector,*bh));
		PRINTK (("get_entry sector apres brelse\n"));
		if (!sector)
			return -1; /* beyond EOF */
		*pos += sizeof(struct msdos_dir_entry);
#ifdef NDAS_CRYPTO
		if (dir->i_ino == MSDOS_ROOT_INO) {
			if (!(*bh = fat_bread_direct(sb, sector))) {
				printk("Directory sread (sector 0x%x) failed\n",sector);
				continue;
			}
		}
		else {
			if (!(*bh = fat_bread(sb, sector))) {
				printk("Directory sread (sector 0x%x) failed\n",sector);
				continue;
			}
		}
#else
		if (!(*bh = fat_bread(sb, sector))) {
			printk("Directory sread (sector 0x%x) failed\n",sector);
			continue;
		}
#endif
		PRINTK (("get_entry apres sread\n"));

		offset &= sb->s_blocksize - 1;
		*de = (struct msdos_dir_entry *) ((*bh)->b_data + offset);
		*i_pos = ((loff_t)sector << sbi->dir_per_block_bits) + (offset >> MSDOS_DIR_BITS);

		return 0;
	}
}


/*
 * Now an ugly part: this set of directory scan routines works on clusters
 * rather than on inodes and sectors. They are necessary to locate the '..'
 * directory "inode". raw_scan_sector operates in four modes:
 *
 * name     number   ino      action
 * -------- -------- -------- -------------------------------------------------
 * non-NULL -        X        Find an entry with that name
 * NULL     non-NULL non-NULL Find an entry whose data starts at *number
 * NULL     non-NULL NULL     Count subdirectories in *number. (*)
 * NULL     NULL     non-NULL Find an empty entry
 *
 * (*) The return code should be ignored. It DOES NOT indicate success or
 *     failure. *number has to be initialized to zero.
 *
 * - = not used, X = a value is returned unless NULL
 *
 * If res_bh is non-NULL, the buffer is not deallocated but returned to the
 * caller on success. res_de is set accordingly.
 *
 * If cont is non-zero, raw_found continues with the entry after the one
 * res_bh/res_de point to.
 */


#define RSS_NAME /* search for name */ \
    done = !strncmp(data[entry].name,name,MSDOS_NAME) && \
     !(data[entry].attr & ATTR_VOLUME);

#define RSS_START /* search for start cluster */ \
    done = !IS_FREE(data[entry].name) \
      && ( \
           ( \
             (sbi->fat_bits != 32) ? 0 : (CF_LE_W(data[entry].starthi) << 16) \
           ) \
           | CF_LE_W(data[entry].start) \
         ) == *number;

#define RSS_FREE /* search for free entry */ \
    { \
	done = IS_FREE(data[entry].name); \
    }

#define RSS_COUNT /* count subdirectories */ \
    { \
	done = 0; \
	if (!IS_FREE(data[entry].name) && (data[entry].attr & ATTR_DIR)) \
	    (*number)++; \
    }

#ifdef NDAS_CRYPTO
static int raw_scan_sector(struct super_block *sb, struct inode *dir, int sector,
			   const char *name, int *number, loff_t *i_pos,
			   struct buffer_head **res_bh,
			   struct msdos_dir_entry **res_de)
#else
static int raw_scan_sector(struct super_block *sb, int sector,
			   const char *name, int *number, loff_t *i_pos,
			   struct buffer_head **res_bh,
			   struct msdos_dir_entry **res_de)
#endif
{
	struct ndasfat_sb_info *sbi = MSDOS_SB(sb);
	struct buffer_head *bh;
	struct msdos_dir_entry *data;
	int entry,start,done;

#ifdef NDAS_CRYPTO
	if (dir->i_ino == MSDOS_ROOT_INO) {
		if (!(bh = fat_bread_direct(sb, sector)))
			return -EIO;
	}
	else {
		if (!(bh = fat_bread(sb, sector)))
			return -EIO;
	}
#else
	if (!(bh = fat_bread(sb, sector)))
		return -EIO;
#endif

	data = (struct msdos_dir_entry *) bh->b_data;
	for (entry = 0; entry < sbi->dir_per_block; entry++) {
/* RSS_COUNT:  if (data[entry].name == name) done=true else done=false. */
		if (name) {
			RSS_NAME
		} else {
			if (!i_pos) RSS_COUNT
			else {
				if (number) RSS_START
				else RSS_FREE
			}
		}
		if (done) {
			if (i_pos) {
				*i_pos = ((loff_t)sector << sbi->dir_per_block_bits) + entry;
			}
			start = CF_LE_W(data[entry].start);
			if (sbi->fat_bits == 32)
				start |= (CF_LE_W(data[entry].starthi) << 16);

			if (!res_bh)
				fat_brelse(sb, bh);
			else {
				*res_bh = bh;
				*res_de = &data[entry];
			}
			return start;
		}
	}
	fat_brelse(sb, bh);
	return -ENOENT;
}


/*
 * raw_scan_root performs raw_scan_sector on the root directory until the
 * requested entry is found or the end of the directory is reached.
 */

#ifdef NDAS_CRYPTO
static int raw_scan_root(struct super_block *sb, struct inode *dir, const char *name,
			 int *number, loff_t *i_pos,
			 struct buffer_head **res_bh,
			 struct msdos_dir_entry **res_de)
#else
static int raw_scan_root(struct super_block *sb, const char *name,
			 int *number, loff_t *i_pos,
			 struct buffer_head **res_bh,
			 struct msdos_dir_entry **res_de)
#endif
{
	int count,cluster;

	for (count = 0;
	     count < MSDOS_SB(sb)->dir_entries / MSDOS_SB(sb)->dir_per_block;
	     count++) {
#ifdef NDAS_CRYPTO
		cluster = raw_scan_sector(sb, dir, MSDOS_SB(sb)->dir_start + count,
					  name, number, i_pos, res_bh, res_de);
#else
		cluster = raw_scan_sector(sb, MSDOS_SB(sb)->dir_start + count,
					  name, number, i_pos, res_bh, res_de);
#endif
		if (cluster >= 0)
			return cluster;
	}
	return -ENOENT;
}


/*
 * raw_scan_nonroot performs raw_scan_sector on a non-root directory until the
 * requested entry is found or the end of the directory is reached.
 */

#ifdef NDAS_CRYPTO
static int raw_scan_nonroot(struct super_block *sb, struct inode *dir, int start, const char *name,
			    int *number, loff_t *i_pos,
			    struct buffer_head **res_bh,
			    struct msdos_dir_entry **res_de)
#else
static int raw_scan_nonroot(struct super_block *sb, int start, const char *name,
			    int *number, loff_t *i_pos,
			    struct buffer_head **res_bh,
			    struct msdos_dir_entry **res_de)
#endif
{
	struct ndasfat_sb_info *sbi = MSDOS_SB(sb);
	int count, cluster, sector;

#ifdef DEBUG
	printk("raw_scan_nonroot: start=%d, cluster_size = %d\n",start, sbi->cluster_size);
#endif
	do {
		for (count = 0; count < sbi->cluster_size; count++) {
			sector = (start - 2) * sbi->cluster_size
				+ count + sbi->data_start;
#ifdef NDAS_CRYPTO
			cluster = raw_scan_sector(sb, dir, sector, name, number,
						  i_pos, res_bh, res_de);
#else
			cluster = raw_scan_sector(sb, sector, name, number,
						  i_pos, res_bh, res_de);
#endif

			if (cluster >= 0)
				return cluster;
		}
		if (!(start = fat_access(sb,start,-1))) {
			fat_fs_panic(sb,"FAT error");
			break;
		}
#ifdef DEBUG
	printk("next start: %d\n",start);
#endif
	}
	while (start != -1);
	return -ENOENT;
}


/*
 * raw_scan performs raw_scan_sector on any sector.
 *
 * NOTE: raw_scan must not be used on a directory that is is the process of
 *       being created.
 */

#ifdef NDAS_CRYPTO
static int raw_scan(struct super_block *sb, struct inode *dir, int start, const char *name,
		    loff_t *i_pos, struct buffer_head **res_bh,
		    struct msdos_dir_entry **res_de)
#else
static int raw_scan(struct super_block *sb, int start, const char *name,
		    loff_t *i_pos, struct buffer_head **res_bh,
		    struct msdos_dir_entry **res_de)
#endif
{
#ifdef NDAS_CRYPTO
	if (start)
		return raw_scan_nonroot(sb,dir,start,name,NULL,i_pos,res_bh,res_de);
	else
		return raw_scan_root(sb,dir,name,NULL,i_pos,res_bh,res_de);
#else
	if (start)
		return raw_scan_nonroot(sb,start,name,NULL,i_pos,res_bh,res_de);
	else
		return raw_scan_root(sb,name,NULL,i_pos,res_bh,res_de);
#endif
}

/*
 * fat_subdirs counts the number of sub-directories of dir. It can be run
 * on directories being created.
 */
#ifdef NDAS_CRYPTO
int fat_subdirs(struct inode *dir, struct inode *parent)
#else
int fat_subdirs(struct inode *dir)
#endif
{
	struct ndasfat_sb_info *sbi = MSDOS_SB(dir->i_sb);
	int number;

	number = 0;
	if ((dir->i_ino == MSDOS_ROOT_INO) && (sbi->fat_bits != 32))
#ifdef NDAS_CRYPTO
		raw_scan_root(dir->i_sb, dir, NULL, &number, NULL, NULL, NULL);
#else
		raw_scan_root(dir->i_sb, NULL, &number, NULL, NULL, NULL);
#endif
	else {
		if ((dir->i_ino != MSDOS_ROOT_INO) && !MSDOS_I(dir)->i_start)
			return 0; /* in mkdir */
		else {
#ifdef NDAS_CRYPTO
			struct super_block *sb = dir->i_sb;
			struct buffer_head *bh = 0;
			int start = MSDOS_I(dir)->i_start;
			int b, offset;

			if ( parent && parent->i_ino == MSDOS_ROOT_INO ) {
				do {
					if (MSDOS_SB(sb)->fat_bits == 32) {
						offset = start*4;
					} else if (MSDOS_SB(sb)->fat_bits == 16) {
						offset = start*2;
					} else {
						offset = start*3/2;
					}

					b = MSDOS_SB(sb)->fat_start + (offset >> sb->s_blocksize_bits);
					bh = fat_bread_direct(sb, b);
					if (!(start = fat_access(sb, start, -1))) {
						fat_fs_panic(sb, "FAT error");
						brelse(bh);
						break;
					}
					brelse(bh);
				} while (start != -1);
			}
#endif

#ifdef NDAS_CRYPTO
			raw_scan_nonroot(dir->i_sb, dir, MSDOS_I(dir)->i_start,
					 NULL, &number, NULL, NULL, NULL);
#else
			raw_scan_nonroot(dir->i_sb, MSDOS_I(dir)->i_start,
					 NULL, &number, NULL, NULL, NULL);
#endif
		}
	}
	return number;
}


/*
 * Scans a directory for a given file (name points to its formatted name) or
 * for an empty directory slot (name is NULL). Returns an error code or zero.
 */

int fat_scan(struct inode *dir, const char *name, struct buffer_head **res_bh,
	     struct msdos_dir_entry **res_de, loff_t *i_pos)
{
	int res;

#ifdef NDAS_CRYPTO
	res = raw_scan(dir->i_sb, dir, MSDOS_I(dir)->i_start, name, i_pos,
		       res_bh, res_de);
#else
	res = raw_scan(dir->i_sb, MSDOS_I(dir)->i_start, name, i_pos,
		       res_bh, res_de);
#endif
	return (res < 0) ? res : 0;
}

#endif

#include "ndasfatproc.h"

#if 0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#endif

MODULE_DESCRIPTION("My kernel module");
MODULE_AUTHOR("root (root@fedora7)");
MODULE_LICENSE("$LICENSE$");

int NdasfatDebugLevel = 0;

SUPERBLOCK_EXTENSION	SuperblockExt;	

static int ndasfat_init_module(void)
{
	int 	result;
	bool	primary;

	printk( KERN_DEBUG "Module ndasfat init\n" );

	result = NdftInit();

	if (result != 0) {

		NDAS_ASSERT( false );
		return result;
	}	

	result = init_fat_fs();
	
	if (result != 0) {

		NDAS_ASSERT( false );
		NdftExit();

		return result;
	}

	result = init_vfat_fs();

	if (result != 0) {

		NDAS_ASSERT( false );
		exit_fat_fs();
		NdftExit();

		return result;
	}

	memset( &SuperblockExt, 0, sizeof(SUPERBLOCK_EXTENSION) );
	mutex_init( &SuperblockExt.FastMutex );
	atomic_set( &SuperblockExt.ReferenceCount, 1 );

	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[0] = 0x00;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[1] = 0x0B;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[2] = 0xD0;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[3] = 0x02;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[4] = 0x8D;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Node[5] = 0xAD;

	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NetdiskAddress.Port = htons(0x2710);

	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo = 0;

	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[0] = 0xBB;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[1] = 0xEA;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[2] = 0x30;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[3] = 0x15;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[4] = 0x73;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[5] = 0x50;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[6] = 0x4A;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.Password[7] = 0x1F;

	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[0] = 0x27;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[1] = 0x10;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[2] = 0x00;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[3] = 0x0B;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[4] = 0xD0;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[5] = 0x02;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[6] = 0x8D;
	SuperblockExt.NetdiskPartitionInformation.NetdiskInformation.NdscId[7] = 0xAD;

	SuperblockExt.NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart = 0x7e00;

	primary = false;

	AddNetdisk( &SuperblockExt.NetdiskPartitionInformation.NetdiskInformation, primary, NULL );

	msleep( 3 * 1000 ); // 3 HZ

	SuperblockExt.Secondary = SecondaryCreate( &SuperblockExt );

	if (SuperblockExt.Secondary == NULL) {

		result = -EIO;

		RemoveNetdisk( &SuperblockExt.NetdiskPartitionInformation.NetdiskInformation );

		exit_vfat_fs();
		exit_fat_fs();
		NdftExit();

		NDASFAT_PRINTK( 0, ("error return\n") );

		return result;
	}

	NDASFAT_PRINTK( 0, ("return\n") );
	
	return 0;
}

VOID
SuperblockExtReference (
	PSUPERBLOCK_EXTENSION	SuperblockExt
	)
{
    LONG result;
	
    result = InterlockedIncrement ( &SuperblockExt->ReferenceCount );

    NDAS_ASSERT( result >= 0 );
}


VOID
SuperblockExtDereference (
	PSUPERBLOCK_EXTENSION	SuperblockExt
	)
{
    LONG result;
	
    result = InterlockedDecrement( &SuperblockExt->ReferenceCount );
    NDAS_ASSERT( result >= 0 );

#if 0

    if (result == 0) {

		PSUPERBLOCK_EXTENSION lfsDeviceExt = Secondary->SuperblockExt;

		ExFreePoolWithTag( Secondary, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Dereference: Secondary is Freed Secondary  = %p\n", Secondary) );
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
					   ("Secondary_Dereference: SuperblockExt->Reference = %d\n", 
						atomic_read(&lfsDeviceExt->ReferenceCount)) );

		SuperblockExtDereference( lfsDeviceExt );
	}

#endif
}

static void ndasfat_exit_module(void)
{
	NDASFAT_PRINTK( 0, ("enter\n") );

	exit_vfat_fs();
	exit_fat_fs();

	RemoveNetdisk( &SuperblockExt.NetdiskPartitionInformation.NetdiskInformation );

	SecondaryClose( SuperblockExt.Secondary );

	NdftExit();

	NDASFAT_PRINTK( 0, ("return\n") );

	printk( KERN_DEBUG "Module ndasfat exit\n" );
}

module_init(ndasfat_init_module);
module_exit(ndasfat_exit_module);

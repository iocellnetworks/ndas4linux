#include "ndasfatproc.h"

NDFT	GlobalNdft = { {0} };

int
NdftThread (
	void	*arg
	);

int 
NdftInit (
	void
	) 
{
	int 	error;
	pid_t 	pid;

	NDASFAT_PRINTK( 0, ("enter\n") );

	init_completion( &GlobalNdft.Thread.Alive );
	init_completion( &GlobalNdft.Thread.Dead );

	atomic_set( &GlobalNdft.Thread.Event, 0 );
	init_waitqueue_head( &GlobalNdft.Thread.WaitQ );

	spin_lock_init( &GlobalNdft.Thread.NIQSpinLock );
	INIT_LIST_HEAD( &GlobalNdft.Thread.NIQueue );
	
	pid = kernel_thread( NdftThread, &GlobalNdft, 0 );

	if (pid > 0) {

		error = 0;
		GlobalNdft.Thread.Pid = pid;
	
	} else {

		NDAS_ASSERT( false );
		error = pid ? pid : -ECHILD;

		return error;
	}

	wait_for_completion( &GlobalNdft.Thread.Alive );

	NDASFAT_PRINTK( 0, ("return\n") );

	return error;
}


void 
NdftExit (
	void
	) 
{
	NDASFAT_PRINTK( 0, ("enter\n") );

	if (GlobalNdft.Thread.Pid) {

		GlobalNdft.Thread.RequestToTerminate = TRUE;

		atomic_inc( &GlobalNdft.Thread.Event );
		wake_up( &GlobalNdft.Thread.WaitQ );
		wait_for_completion( &GlobalNdft.Thread.Dead );
		GlobalNdft.Thread.Pid = 0;
	}

	NDAS_ASSERT( list_empty(&GlobalNdft.Thread.NIQueue) );

	NDASFAT_PRINTK( 0, ("return\n") );

	return;
}

int 
QueryPrimary (
	PNETDISK_INFORMATION	NetdiskInformation,
	struct sockaddr_lpx		*PrimaryAddress
	)
{
	int 						error;
	PNDFT_NETDISK_INFORMATION	ndftNetdiskInformation;

	error = -ENOENT;

	spin_lock( &GlobalNdft.Thread.NIQSpinLock );

	list_for_each_entry(ndftNetdiskInformation, &GlobalNdft.Thread.NIQueue, ListHead) {

		if (FlagOn(ndftNetdiskInformation->Flags, NDFTNI_FLAGS_PRIMARY_ADDRESS_SET)) {

			if (memcmp(&ndftNetdiskInformation->NetdiskInformation,
					   NetdiskInformation,
					   sizeof(NETDISK_INFORMATION)) == 0) {

				memcpy( PrimaryAddress,
						&ndftNetdiskInformation->PrimaryAddress,
						sizeof(struct sockaddr_lpx) );

				error = 0;
				break;
			}
		}
	}

	spin_unlock( &GlobalNdft.Thread.NIQSpinLock );

	return error;
}

int 
AddNetdisk (
	PNETDISK_INFORMATION	NetdiskInformation,
	bool					Primary,
	struct sockaddr_lpx		*PrimaryAddress
	)
{
	PNDFT_NETDISK_INFORMATION	ndftNetdiskInformation;

	ndftNetdiskInformation = kmalloc( sizeof(NDFT_NETDISK_INFORMATION), GFP_KERNEL );

	if (!ndftNetdiskInformation) {

		NDAS_ASSERT( false );
		return -ENOMEM;
	}

	memset( ndftNetdiskInformation, 0, sizeof(NDFT_NETDISK_INFORMATION) );

	INIT_LIST_HEAD( &ndftNetdiskInformation->ListHead );

	memcpy( &ndftNetdiskInformation->NetdiskInformation, 
			NetdiskInformation, 
			sizeof(NETDISK_INFORMATION) );

	if (Primary == true) {

		SetFlag( ndftNetdiskInformation->Flags, NDFTNI_FLAGS_LOCAL_PRIMARY );
		SetFlag( ndftNetdiskInformation->Flags, NDFTNI_FLAGS_PRIMARY_ADDRESS_SET );

		memcpy( &ndftNetdiskInformation->PrimaryAddress,
				PrimaryAddress,
				sizeof(struct sockaddr_lpx) );
	}

	spin_lock( &GlobalNdft.Thread.NIQSpinLock );
	list_add_tail( &ndftNetdiskInformation->ListHead, &GlobalNdft.Thread.NIQueue );
	spin_unlock( &GlobalNdft.Thread.NIQSpinLock );

	return 0;
}

void 
RemoveNetdisk (
	PNETDISK_INFORMATION	NetdiskInformation
	)
{
	PNDFT_NETDISK_INFORMATION	ndftNetdiskInformation;

	spin_lock( &GlobalNdft.Thread.NIQSpinLock );

	list_for_each_entry(ndftNetdiskInformation, &GlobalNdft.Thread.NIQueue, ListHead) {

		if (memcmp(&ndftNetdiskInformation->NetdiskInformation,
				   NetdiskInformation,
				   sizeof(NETDISK_INFORMATION)) == 0) {

			list_del( &ndftNetdiskInformation->ListHead );	
			kfree( ndftNetdiskInformation );

			break;
		}
	}

	spin_unlock( &GlobalNdft.Thread.NIQSpinLock );

	return;
}	

int
NdftThread (
	void	*arg
	)
{
	PNDFT	Ndft = arg;
	bool	ndftThreadExit = false;

	int		error = 0;

	struct sockaddr_lpx listenAddress;
	int					sockFd;

	NDASFAT_PRINTK( 0, ("enter\n") );

	daemonize( "ndft_thread" );

	error = 0;

	do {

	    sockFd = lpx_socket(LPX_TYPE_DATAGRAM, 0);

		if (sockFd < 0) {

			error = -1;
			break;
    	}

	    memset( &listenAddress, 0, sizeof(listenAddress) );
    	listenAddress.slpx_family = AF_LPX;
		listenAddress.slpx_port = htons(0x0003);

    	error = lpx_bind( sockFd, (struct sockaddr_lpx *) &listenAddress, sizeof(listenAddress) );
	
		if (error != 0) {
	
			break;
		}
	
		error = lpx_set_rtimeout( sockFd, 1000 );	// 1 second

	} while (0);

	if (error != 0) {

		NDAS_ASSERT( false );

		if (sockFd > 0) {
			
			lpx_close( sockFd );
		}

		complete( &Ndft->Thread.Alive );
		complete_and_exit( &Ndft->Thread.Dead, 0 );
	}

	complete( &Ndft->Thread.Alive );

	do {

		int 				error;
		struct sockaddr_lpx fromAddress;
		int					fromLen;

		PNDFT_NETDISK_INFORMATION	ndftNetdiskInformation;
		
		//error = wait_event_interruptible_timeout( Ndft->Thread.WaitQ, 
		//										   atomic_read(&Ndft->Thread.Event), 
		//										   HZ );
		
		error = lpx_recvfrom( sockFd, 
							   (void *)Ndft->Thread.Buffer, 
							   sizeof(Ndft->Thread.Buffer), 
							   0, 
							   &fromAddress, 
							   &fromLen );

		spin_lock( &Ndft->Thread.NIQSpinLock );

		list_for_each_entry(ndftNetdiskInformation, &Ndft->Thread.NIQueue, ListHead) {

			if (FlagOn(ndftNetdiskInformation->Flags, NDFTNI_FLAGS_LOCAL_PRIMARY)) {

				continue;
			}

			if (!FlagOn(ndftNetdiskInformation->Flags, NDFTNI_FLAGS_PRIMARY_ADDRESS_SET)) {

				continue;
			}

			if (ndftNetdiskInformation->AddressTimeout < jiffies) {

				ClearFlag( ndftNetdiskInformation->Flags, NDFTNI_FLAGS_PRIMARY_ADDRESS_SET );
			}
		}

		spin_unlock( &Ndft->Thread.NIQSpinLock );
		
		if (error == 0) { // timeout

			if ( atomic_read(&Ndft->Thread.Event) == 0 ) {

				continue;
			}
		}

		NDASFAT_PRINTK( 4, ("error = %d sizeof(NDFT_HEADER) = %d, "
							"sizeof(NDFT_PRIMARY_UPDATE) = %d\n", 
							error, sizeof(NDFT_HEADER), sizeof(NDFT_PRIMARY_UPDATE)) );

		if (error != sizeof(NDFT_HEADER) + sizeof(NDFT_PRIMARY_UPDATE)) {

			NDAS_ASSERT(false);
		
		} else {

			PNDFT_HEADER				ndftHeader;
			PNDFT_PRIMARY_UPDATE		ndftPrimaryUpdate;

			ndftHeader = (PNDFT_HEADER)Ndft->Thread.Buffer;
			ndftPrimaryUpdate = (PNDFT_PRIMARY_UPDATE)&Ndft->Thread.Buffer[sizeof(NDFT_HEADER)];

			spin_lock( &GlobalNdft.Thread.NIQSpinLock );

			list_for_each_entry(ndftNetdiskInformation, &Ndft->Thread.NIQueue, ListHead) {

				if (!FlagOn(ndftNetdiskInformation->Flags, NDFTNI_FLAGS_LOCAL_PRIMARY)) {

					if (memcmp(&ndftNetdiskInformation->NetdiskInformation.NetdiskAddress.Node,
							   ndftPrimaryUpdate->NetdiskNode,
							   6) == 0) {
				
						memcpy( &ndftNetdiskInformation->PrimaryAddress.slpx_node,
								&fromAddress.slpx_node,
								6 );
						
						NDAS_ASSERT( ndftPrimaryUpdate->PrimaryPort == htons(0x0001) );
						NDAS_ASSERT( ndftPrimaryUpdate->NetdiskPort == htons(0x2710) );

						ndftNetdiskInformation->PrimaryAddress.slpx_port = ndftPrimaryUpdate->PrimaryPort;

						ndftNetdiskInformation->PrimaryAddress.slpx_family = AF_LPX; 

						ndftNetdiskInformation->AddressTimeout = jiffies + msecs_to_jiffies(5000);

						SetFlag( ndftNetdiskInformation->Flags, NDFTNI_FLAGS_PRIMARY_ADDRESS_SET );

						NDASFAT_PRINTK( 3, ("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
										ndftPrimaryUpdate->NdscId[0], ndftPrimaryUpdate->NdscId[1],
										ndftPrimaryUpdate->NdscId[2], ndftPrimaryUpdate->NdscId[3],
										ndftPrimaryUpdate->NdscId[4], ndftPrimaryUpdate->NdscId[5],
										ndftPrimaryUpdate->NdscId[6], ndftPrimaryUpdate->NdscId[7]) );

						NDASFAT_PRINTK( 4, ("ndftPrimaryUpdate->NetdiskPort %04X\n",
											ndftPrimaryUpdate->NetdiskPort) );

						break;
					}
				}
			}

			spin_unlock( &Ndft->Thread.NIQSpinLock );
		}
		
		if (atomic_read(&Ndft->Thread.Event) == 0) {

			continue;
		}

		atomic_set( &Ndft->Thread.Event, 0 );

		if (Ndft->Thread.RequestToTerminate) {

			ndftThreadExit = true;
			continue;
		}

	} while( !ndftThreadExit );

	NDASFAT_PRINTK( 0, ("exit\n") );

	lpx_close( sockFd );

	complete_and_exit( &Ndft->Thread.Dead, 0 );
}


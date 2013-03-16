#ifndef __NDFT_H__
#define __NDFT_H__

typedef struct _NDFT_NETDISK_INFORMATION {

#define NDFTNI_FLAGS_LOCAL_PRIMARY			0x00000001
#define NDFTNI_FLAGS_PRIMARY_ADDRESS_SET	0x00000002

	ulong				Flags;
	struct list_head	ListHead;
	NETDISK_INFORMATION	NetdiskInformation;
	
	struct sockaddr_lpx	PrimaryAddress;
	ulong				AddressTimeout;

} NDFT_NETDISK_INFORMATION, *PNDFT_NETDISK_INFORMATION;

typedef struct _NDFT {

	struct {

#define NDFT_FLAGS_RUNNING	0x00000001
#define NDFT_FLAGS_STOP		0x00000002

#define NDFT_FLAGS_ERROR	0x80000000

		ulong	Flags;
		bool	RequestToTerminate;

		pid_t	Pid;

		struct completion Alive;
		struct completion Dead;

		atomic_t			Event;
		wait_queue_head_t 	WaitQ;

		spinlock_t			NIQSpinLock;
		struct list_head	NIQueue;
	
		char	Buffer[2000];
	
	} Thread;

} NDFT, *PNDFT;

extern NDFT	GlobalNdft;

#endif

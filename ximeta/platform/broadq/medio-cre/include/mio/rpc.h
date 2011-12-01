#ifndef RPC_H
#define RPC_H

#define ipRPC_ID  0x80000664


#define smapqRPC_ID (('s'<<24)||('m'<<24)||('a'<<9)||('p'<<0))
#define SMAPQ_RPCBUF_SIZE (1024)
#define SMAPQ_RPC_DHCP    (1)
#define SMAPQ_RPC_IP      (2)
#define SMAPQ_RPC_GETIP   (3)


#define IPFS_RPCBUF_SIZE 2048
#define ipfsRPC_ID 0x80000555

#define IPFS_RPC_BENCHMARK  (0)
#define IPFS_RPC_CONNECT    (1)
#define IPFS_RPC_DISCONNECT (2)


#endif

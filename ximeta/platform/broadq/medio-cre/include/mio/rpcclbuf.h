
#ifndef __MIO_RPC_CLIENT_BUFFER_MANAGER_H__
#define  __MIO_RPC_CLIENT_BUFFER_MANAGER_H__
#include <map>
using namespace std;
#include <tamtypes.h>
#include <sifrpc.h>
#include <mio/pseudoprocs.h>


class MioRpcClientBufferMgr {
public:
    typedef int (*OutputHandler)(const char *format, ...);
    enum HowMuch {NONE, CLIENT_DATA};
    MioRpcClientBufferMgr(u32 rpcId);
    ~MioRpcClientBufferMgr();

    void allocate(MioPid pid = 0);
    void deallocate(MioPid pid = 0);
    SifRpcClientData_t& operator[](MioPid pid) const;
    void *xferBuffer(MioPid pid) const;
    void print(HowMuch, OutputHandler = printf) const;
    void print(MioPid pid, HowMuch, OutputHandler = printf) const;

private:
    MioRpcClientBufferMgr(const MioRpcClientBufferMgr&); // disable use
    MioRpcClientBufferMgr& operator=(const MioRpcClientBufferMgr&); // disable use
    u32 myRpcId;
    SifRpcClientData_t *myBuffers[MioProcessTable::MAX_PID];
    void *myXferBuffers[MioProcessTable::MAX_PID];
};

#endif // __MIO_RPC_CLIENT_BUFFER_MANAGER_H__

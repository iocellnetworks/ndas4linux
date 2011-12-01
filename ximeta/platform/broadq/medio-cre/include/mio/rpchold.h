
#ifndef __MIO_RPC_CLIENT_BUFFER_MANAGER_H__
#define  __MIO_RPC_CLIENT_BUFFER_MANAGER_H__
#include <map>
using namespace std;

class RpcClientBuffer; // PS2 type...
typedef RpcClientBuffer* (*RpcClientBufferMaker)();

template<class Key, class Factory>
class MioRpcClientBufferMgr {
public:
    typedef Key key_type;
    typedef Factory factory_type;

    MioRpcClientBufferMgr() { };
    ~MioRpcClientBufferMgr() { };

    void addFactory(const key_type&, factory_type::maker_type);
    void removeFactory(const key_type&);

    RpcClientBuffer& operator[](const key_type&) const;

private:
    map<key_type, factory_type::buffer_type> myFactories;
};

#include <mio/rpcclbuf.cc>

#endif // __MIO_RPC_CLIENT_BUFFER_MANAGER_H__

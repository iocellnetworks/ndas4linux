
#include <stdio.h>
#include <mio/rpcclbuf.h>

template<class Key, class Factory>
void
MioRpcClientBufferMgr<Key,Factory>::addFactory(const Key& key, RpcClientBufferMaker factory)
{
    myFactories[key] = factory;
}


template<class Key, class Factory>
RpcClientBuffer&
MioRpcClientBufferMgr<Key,Factory>::operator[](const Key& key) const
{
    // get 
    return myFactories[key];
}

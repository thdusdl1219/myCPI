#ifndef __SERVER_H__
#define __SERVER_H__

#include "qsocket.h"
#include "../comm/comm_manager.h"
#include <cassert>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace std;
namespace corelab {
  namespace UVA {
    extern "C" void UVAServerCallbackSetter(CommManager *comm);
    extern "C" void UVAServerInitializer(CommManager *comm_);
    extern "C" void UVAServerFinalizer();
    //void* ServerOpenRoutine(void*);
    //void* ClientRoutine(void*);
    void newfaceHandler(void*, uint32_t, uint32_t);
    void heapAllocHandler(void*, uint32_t, uint32_t);
    void loadHandler(void*, uint32_t, uint32_t);
    void storeHandler(void*, uint32_t, uint32_t);
    void mmapHandler(void*, uint32_t, uint32_t);
    void memsetHandler(void*, uint32_t, uint32_t);
    void memcpyHandler(void*, uint32_t, uint32_t);
    void memcpyHandlerForHLRC(void*, uint32_t, uint32_t);
    void heapSegfaultHandler(void*, uint32_t, uint32_t);
    void globalSegfaultHandler(void*, uint32_t, uint32_t);
    void globalInitCompleteHandler(void*, uint32_t, uint32_t);
    void acquireHandler(void*, uint32_t, uint32_t);
    void releaseHandler(void*, uint32_t, uint32_t);
    void syncHandler(void*, uint32_t, uint32_t);

#if 0
    /* These two function do nothing. Everybody is client */
    extern "C" void uva_server_load(void *addr, size_t len);
    extern "C" void uva_server_store(void *addr, size_t len, void *data); 
#endif
    
    /* RuntimeClientConnTb: this map record "ClientId" as a key and "QSocket"
     * as a value in runtime when a client comes in. 
     *
     * isInitEnd: this value check that global init is end. Server runtime
     * makes this value true when global initializer send complete signal.
     * Server runtime can broadcast start permission signal to non-initializer
     * clients. If some clients don't connect after this value become true, it
     * is fine. Server runtime can immediately give start permission to late
     * client. 
     *
     *   written by Bongjun.
     */
    //static std::map<int *, QSocket *> *RuntimeClientConnTb;
    static std::vector<uint32_t> *RuntimeClientConnTb;
    static bool isInitEnd = false;


    struct pageInfo {
      set<int>* accessS;
      pageInfo() {
        accessS = new set<int>;
      }
      ~pageInfo() {
        delete accessS;
      }
    };

    // first argumant in map is address / 0x1000(PAGE_SIZE) 
    static map<long, struct pageInfo*> *pageMap;
  }
}

#endif

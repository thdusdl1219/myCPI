#ifndef __SERVER_H__
#define __SERVER_H__

#include "qsocket.h"
#include <map>
#include <set>

using namespace std;
namespace corelab {
  namespace UVA {
    extern "C" void UVAServerInitialize();
    extern "C" void UVAServerFinalize();
    void* ServerOpenRoutine(void*);
    void* ClientRoutine(void*);
    void heapAllocHandler(int*);
    void loadHandler(int*);
    void storeHandler(int*);
    void mmapHandler(int*);
    void memsetHandler(int*);
    void memcpyHandler(int*);
    void memcpyHandlerForHLRC(int*);
    void heapSegfaultHandler(int*);
    void globalSegfaultHandler(int*);
    void globalInitCompleteHandler(int*);
    void invalidHandler(int*);
    void releaseHandler(int*);
    void syncHandler(int*);


    /* These two function do nothing. Everybody are client */
    extern "C" void uva_server_load(void *addr, size_t len);
    extern "C" void uva_server_store(void *addr, size_t len, void *data); 
    
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
    static std::map<int *, QSocket *> *RuntimeClientConnTb;
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

#ifndef __SERVER_H__
#define __SERVER_H__

#include "qsocket.h"
#include <map>

namespace corelab {
  namespace UVA {
    extern "C" void UVAServerInitialize();
    extern "C" void UVAServerFinalize();
    void* ServerOpenRoutine(void*);
    void* ClientRoutine(void*);

    /* These two function do nothing. Everybody are client */
    extern "C" void uva_server_load(void *addr, size_t len);
    extern "C" void uva_server_store(void *addr, size_t len, void *data); 
    
    /* RuntimeClientConnTb: this map record "ClientId" as a key and "QSocket"
     * as a value in runtime when a client comes in. 
     */
    static std::map<int *, QSocket *> *RuntimeClientConnTb;

    static bool isInitEnd = false;
  }
}

#endif

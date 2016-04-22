#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <cassert>

#include "mm.h"
#include "qsocket.h"
#include "server.h"
#include "log.h"
#include "hexdump.h"

#define TEST 1

using namespace corelab::XMemory;
namespace corelab {
  namespace UVA {

    static QSocket* socket;
    pthread_t openThread; 

    extern "C" void UVAServerInitialize() {
        LOG("UVA manager(server) : initialize\n");
        pthread_create(&openThread, NULL, ServerOpenRoutine, NULL); 
    }

    extern "C" void UVAServerFinalize() {
        pthread_join(openThread, NULL);
    }
    void* ServerOpenRoutine(void *) {
      LOG("UVA manager(server) : after pthread_create serveropenroutine\n");
      char port[10];

#if TEST
        strcpy(port, "5959");
#else
        fprintf(stderr, "system : Enter port to listen : ");
        scanf("%s", port);
#endif

      socket = new QSocket ();
      socket->setClientRoutine(ClientRoutine);
      socket->open(port);
      fprintf (stderr, "system : client connected.\n");
      return NULL;
    }

    void* ClientRoutine(void * data) {
      int *clientId = (int *)data;
      pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
      int mode;
      int datalen;
      int rval = 0;
      void* HeapTop;
      void * allocAddr = NULL;
      char *valOfRequestedAddr = NULL;

      while(true) {
        pthread_mutex_lock(&mutex);
        socket->receiveQue(clientId);
        mode = socket->takeWordF(clientId);
        datalen = socket->takeWordF(clientId);
        fprintf(stderr, "[system] (mode, datalen) : (%d , %d)\n", mode, datalen);
        int lenbuf;
        uint64_t lenType;
        void *requestedAddr;
        void *valueToStore;
        switch(mode) {
          case -1 :
            pthread_exit(&rval);
            fprintf(stderr, "[system] thread exit!");
            break;
          case 0 :
            socket->takeRange(&lenbuf, datalen, clientId);
            fprintf(stderr, "[system] (takeRange) : (%d)\n", lenbuf);
            // memory operation
            
            HeapTop = XMemoryManager::getHeapTop(); 
            fprintf(stderr, "[system] heapTop : %p\n", HeapTop);
            // allocAddr = XMemoryManager::allocate(lenbuf, true);
            allocAddr = XMemoryManager::allocateServer(HeapTop, lenbuf);
            HeapTop = XMemoryManager::getHeapTop(); 
            fprintf(stderr, "[system] allocAddr : (%p)\n", allocAddr);
            fprintf(stderr, "[system] heapTop : %p\n", HeapTop);
            
            // memory operation end
            socket->pushWordF(1, clientId);
            socket->pushWordF(sizeof(allocAddr), clientId);
            socket->pushRangeF(&allocAddr, sizeof(allocAddr), clientId);
            socket->sendQue(clientId);
            break;
          case 1 :
            assert(false && "something is strange!");
            break;
          case 2 :
            LOG("[server] get Load request from client\n");
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length : %d\n", lenType);
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr : (%p)\n", requestedAddr);
            
           
            //valOfRequestedAddr = (char*)malloc(lenType);
            //memcpy(valOfRequestedAddr, requestedAddr, lenType);
            //LOG("[server] v[0]:%c, v[1]:%c\n", valOfRequestedAddr[0], valOfRequestedAddr[1]);
            //LOG("[server] size %d, value %s\n", sizeof(valOfRequestedAddr), *requestedAddr); 
            // load latest value from requested address
            socket->pushWordF(3, clientId);
            socket->pushWordF(lenType, clientId);
            socket->pushRangeF(requestedAddr, lenType, clientId);
            LOG("[server] TEST loaded value : %d\n", *((int*)requestedAddr));
            //LOG("[server] val of addr : ");
            //for(int i=0; i<lenType; i++) {
            //  printf("%02x", ((unsigned char*)requestedAddr)[i]);
            //}
            //printf("\n");
            socket->sendQue(clientId);
            break;
          case 3 :
            assert(false && "something is strange!");
            break;
          case 4 :
            LOG("[server] get store request from client\n");
            //lenType = sock
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length : %d\n", lenType);
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr : (%p)\n", requestedAddr);
            valueToStore = malloc(lenType);
            socket->takeRangeF(valueToStore, lenType, clientId);
            LOG("[server] TEST stored value : %d\n", *((int*)valueToStore));
            
            memcpy(requestedAddr, valueToStore, lenType);
            socket->pushWordF(5, clientId);
            socket->pushWordF(0, clientId); // ACK ( 0: normal, -1: abnormal )
            socket->sendQue(clientId);
            hexdump(requestedAddr, lenType);
            XMemory::XMemoryManager::dumpRange(requestedAddr, lenType);
            break;
        }
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    }

    extern "C" void uva_server_load(void *addr, uint64_t len) {
      LOG("[server] Load instr, addr %p, len %d\n", addr, len); 
    }

    extern "C" void uva_server_store(void *addr, uint64_t len, void *data) {
      LOG("[server] Store instr, addr %p, len %d\n", addr, len); 
    }
  }
}

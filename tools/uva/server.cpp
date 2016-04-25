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
#include "xmem_info.h"

#define TEST 1

using namespace corelab::XMemory;
namespace corelab {
  namespace UVA {

    enum {
      THREAD_EXIT = -1,
      HEAP_ALLOC_REQ = 0,
      LOAD_REQ = 2,
      STORE_REQ = 4,
      MMAP_REQ = 6,
      GLOBAL_SEGFAULT_REQ = 8
    };
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
        strcpy(port, "20000");
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
      void *HeapTop;
      void *allocAddr = NULL;
      char *valOfRequestedAddr = NULL;

      void *ptNoConstBegin;
      void *ptNoConstEnd;
      void *ptConstBegin;
      void *ptConstEnd;

      uintptr_t target;

      while(true) {
        pthread_mutex_lock(&mutex);
        socket->receiveQue(clientId);
        mode = socket->takeWordF(clientId);
        LOG("[server] *** Receive message from client (id: %d, mode %d) ***\n", *clientId, mode);  
        int lenbuf;
        size_t lenType;
        size_t sizeOfLength;
        size_t lenMmap;
        void *requestedAddr;
        void *valueToStore;
        switch(mode) {
          case THREAD_EXIT:
            pthread_exit(&rval);
            LOG("[server] thread exit!");
            break;
          case HEAP_ALLOC_REQ: /*** heap allocate request ***/
            datalen = socket->takeWordF(clientId);
            LOG("[server] (datalen) : (%d)\n", datalen);
            socket->takeRange(&lenbuf, datalen, clientId);
            LOG("[server] (takeRange) : (%d)\n", lenbuf);
            // memory operation
            
            HeapTop = XMemoryManager::getHeapTop(); 
            LOG("[server] old heapTop : %p\n", HeapTop);
            // allocAddr = XMemoryManager::allocate(lenbuf, true);
            allocAddr = XMemoryManager::allocateServer(HeapTop, lenbuf);
            HeapTop = XMemoryManager::getHeapTop(); 
            LOG("[server] allocAddr : (%p)\n", allocAddr);
            LOG("[server] new heapTop : %p\n", HeapTop);
            
            // memory operation end
            socket->pushWordF(1, clientId);
            socket->pushWordF(sizeof(allocAddr), clientId);
            socket->pushRangeF(&allocAddr, sizeof(allocAddr), clientId);
            socket->sendQue(clientId);
            break;
          case LOAD_REQ: /*** load request ***/
            datalen = socket->takeWordF(clientId);
            LOG("[server] (datalen) : (%d)\n", datalen);
            LOG("[server] get Load request from client (id: %d)\n", *clientId);

            // receive type length (how much load in byte)
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length (how much): %d\n", lenType);

            // receive requested addr (where)
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
            
            // send ack with value (what to load)
            socket->pushWordF(3, clientId);
            socket->pushWordF(lenType, clientId);
            socket->pushRangeF(requestedAddr, lenType, clientId);
            LOG("[server] TEST loaded value (what): %d\n", *((int*)requestedAddr));
            socket->sendQue(clientId);
            break;
          case STORE_REQ: /*** store request ***/
            datalen = socket->takeWordF(clientId);
            LOG("[server] (datalen) : (%d)\n", datalen);
            LOG("[server] get store request from client\n");

            // get type length (how much store in byte)
            lenType = socket->takeWordF(clientId);
            LOG("[server] type length (how much store in byte): %d\n", lenType);

            // get requested addr (where)
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);

            // get value which client want to store (what to store)
            valueToStore = malloc(lenType);
            socket->takeRangeF(valueToStore, lenType, clientId);
            LOG("[server] TEST stored value (what): %d\n", *((int*)valueToStore));
            
            // store value in UVA address.
            memcpy(requestedAddr, valueToStore, lenType);

            // send ack
            socket->pushWordF(5, clientId); // ACK
            socket->pushWordF(0, clientId); // ACK ( 0: normal, -1: abnormal )
            socket->sendQue(clientId);

            // test
            //hexdump(requestedAddr, lenType);
            xmemDumpRange(requestedAddr, lenType);
            break;
          case MMAP_REQ: /*** mmap request ***/
            datalen = socket->takeWordF(clientId);
            LOG("[server] (datalen) : (%d)\n", datalen);
            LOG("[server] get mmap request from client\n");

            // get requested addr (where)
            socket->takeRangeF(&requestedAddr, datalen, clientId);
            LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
            
            // get size variable's length (32 or 64 bits)
            sizeOfLength = socket->takeWordF(clientId);

            // get length (how much mmap)
            socket->takeRangeF(&lenMmap, sizeOfLength, clientId);
            LOG("[server] mmap length (how much mmap in byte): %d\n", lenMmap);
    
            allocAddr = xmemPagemap(requestedAddr, lenMmap, true);
            
            assert(allocAddr != NULL && "mmap alloc failed in server");

            socket->pushWordF(7, clientId); // ACK
            socket->pushWordF(0, clientId); // ACK (0: normal, -1:abnormal)
            socket->sendQue(clientId);
            break;
          case GLOBAL_SEGFAULT_REQ:
            LOG("[server] get GLOBAL_SEGFALUT_REQ from client (%d)\n", *clientId);
            socket->takeRangeF(&ptNoConstBegin, sizeof(void*), clientId);
            target = (uintptr_t)(&ptNoConstBegin);
            LOG("[server] TEST ptConstBegin (%p)\n", (void*)(*((uintptr_t *)target)));
            socket->takeRangeF(&ptNoConstEnd, sizeof(void*), clientId);
            //socket->takeRangeF(&ptConstBegin, sizeof(void*), clientId);
            //socket->takeRangeF(&ptConstEnd, sizeof(void*), clientId);
            
            socket->pushRangeF((void*)(*((uintptr_t *)(uintptr_t)(&ptNoConstBegin))),
                (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin, clientId);
            //socket->pushRangeF((void*)(*((uintptr_t *)(uintptr_t)(&ptConstBegin))), (uintptr_t)ptConstEnd - (uintptr_t)ptConstBegin, clientId);
            socket->sendQue(clientId);
            LOG("[server] GLOBAL_SEGFALUT_REQ process end (%d)\n", *clientId);
            break;
          default:
            assert(0 && "wrong request mode");
            break;
        }
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    }

    // These two function may not be used.
    extern "C" void uva_server_load(void *addr, size_t len) {
      LOG("[server] Load instr, addr %p, len %d\n", addr, len); 
    }

    extern "C" void uva_server_store(void *addr, size_t len, void *data) {
      LOG("[server] Store instr, addr %p, len %d\n", addr, len); 
    }
  }
}

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
#include "uva_macro.h"

//#define DEBUG_UVA

#define TEST 1

using namespace corelab::XMemory;
namespace corelab {
  namespace UVA {

    static QSocket* socket;
    pthread_t openThread; 

    extern "C" void UVAServerInitialize() {
#ifdef DEBUG_UVA
      LOG("UVA manager(server) : initialize\n");
#endif
      RuntimeClientConnTb = new map<int *, QSocket *>(); 
      assert(!isInitEnd && "When server init, isInitEnd value should be false.");
      pthread_create(&openThread, NULL, ServerOpenRoutine, NULL);
    }

    extern "C" void UVAServerFinalize() {
      pthread_join(openThread, NULL);
    }

    void* ServerOpenRoutine(void *) {
#ifdef DEBUG_UVA
      LOG("UVA manager(server) : after pthread_create serveropenroutine\n");
#endif
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
#ifdef DEBUG_UVA
      fprintf (stderr, "system : client connected.\n");
#endif
      return NULL;
    }

    void* ClientRoutine(void * data) {
#ifdef DEBUG_UVA
      printf("[SERVER] client Routine called (%d)\n", *((int*)data));
#endif
      (*RuntimeClientConnTb)[(int*)data] = socket; 
      if (isInitEnd) {
#ifdef DEBUG_UVA
        printf("[SERVER] Oh.. you are late (This client comes in after glb init finished\n");
#endif
        socket->pushWordF(1, (int*)data);
        socket->sendQue((int*)data);
      }
      int *clientId = (int *)data;
      pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
      int mode;
      int rval = 0;


      /* for memset, memcpy */

      /* // for memset, memcpy */

      while(true) {
        pthread_mutex_lock(&mutex);
        socket->receiveQue(clientId);
        mode = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
        LOG("[server] *** Receive message from client (id: %d, mode %d) ***\n", *clientId, mode);  
#endif

        switch(mode) {
          case THREAD_EXIT:
            pthread_exit(&rval);
#ifdef DEBUG_UVA
            LOG("[server] thread exit!");
#endif
            break;

          case HEAP_ALLOC_REQ: /*** heap allocate request ***/
            heapAllocHandler(clientId); 
            break;

          case LOAD_REQ: /*** load request ***/
            loadHandler(clientId);
            break;

          case STORE_REQ: /*** store request ***/
            storeHandler(clientId);
            break;

          case MMAP_REQ: /*** mmap request ***/
            mmapHandler(clientId);
            break;
          case MEMSET_REQ:
            memsetHandler(clientId);
            break;
          case MEMCPY_REQ:
            memcpyHandler(clientId);
            break;
          case GLOBAL_SEGFAULT_REQ:
            globalSegfaultHandler(clientId);
            break;
          case GLOBAL_INIT_COMPLETE_SIG:
            globalInitCompleteHandler(clientId);
            break;
          case INVALID_REQ:
            invalidHandler(clientId);
          default:
            assert(0 && "wrong request mode");
            break;
        }
        pthread_mutex_unlock(&mutex);
      }
      return NULL;
    }

    void invalidHandler(int* clientId) {
      list<long> sendAddrList;
      for(map<long, struct pageInfo*>::iterator it = pageMap.begin(); it != pageMap.end(); it++) {
        set<int> my_var = it->second->accessS;
        if(my_var.find(*clientId) != my_var.end()) 
           sendAddrList.push_back(it->first);
      }
      // TODO start here!
      //socket->sendWord();


    }


    void heapAllocHandler(int* clientId) {
      int lenbuf;
      int datalen = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
      LOG("[server] (datalen) : (%d)\n", datalen);
#endif
      socket->takeRange(&lenbuf, datalen, clientId);
#ifdef DEBUG_UVA
      LOG("[server] (takeRange) : (%d)\n", lenbuf);
#endif
      // memory operation

      void* HeapTop = XMemoryManager::getHeapTop(); 
#ifdef DEBUG_UVA
      LOG("[server] old heapTop : %p\n", HeapTop);
#endif
      // allocAddr = XMemoryManager::allocate(lenbuf, true);
      void* allocAddr = XMemoryManager::allocateServer(HeapTop, lenbuf);
      HeapTop = XMemoryManager::getHeapTop(); 
#ifdef DEBUG_UVA
      LOG("[server] allocAddr : (%p)\n", allocAddr);
      LOG("[server] new heapTop : %p\n", HeapTop);
#endif
      // insert pageTable into pageMap
      struct pageInfo* newPageInfo = (struct pageInfo *)malloc(sizeof(struct pageInfo));
      newPageInfo->accessS.insert(-1);
      pageMap.insert(map<long, struct pageInfo*>::value_type((long)allocAddr / 1000, newPageInfo));
      
      // memory operation end
      socket->pushWordF(HEAP_ALLOC_REQ_ACK, clientId);
      socket->pushWordF(sizeof(allocAddr), clientId);
      socket->pushRangeF(&allocAddr, sizeof(allocAddr), clientId);
      socket->sendQue(clientId);

    }

    void loadHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get Load request from client (id: %d)\n", *clientId);
#endif

      // receive type length (how much load in byte)
      size_t lenType = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
      LOG("[server] type length (how much): %d\n", lenType);
#endif

      // receive requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // send ack with value (what to load)
      socket->pushWordF(LOAD_REQ_ACK, clientId);
      socket->pushWordF(lenType, clientId);
      socket->pushRangeF(requestedAddr, lenType, clientId);
#ifdef DEBUG_UVA
      LOG("[server] TEST loaded value (what): %d\n", *((int*)requestedAddr));
#endif
      socket->sendQue(clientId);
    }
    void storeHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get store request from client\n");
#endif

      // get type length (how much store in byte)
      size_t lenType = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
      LOG("[server] type length (how much store in byte): %d\n", lenType);
#endif

      // get requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // get value which client want to store (what to store)
      void* valueToStore = malloc(lenType);
      socket->takeRangeF(valueToStore, lenType, clientId);
#ifdef DEBUG_UVA
      LOG("[server] TEST stored value (what): %d\n", *((int*)valueToStore));
#endif

      // store value in UVA address.
      memcpy(requestedAddr, valueToStore, lenType);

      // send ack
      socket->pushWordF(STORE_REQ_ACK, clientId); // ACK
      socket->pushWordF(0, clientId); // ACK ( 0: normal, -1: abnormal ) FIXME useless
      socket->sendQue(clientId);

#ifdef DEBUG_UVA
      // test
      hexdump("store", requestedAddr, lenType);
      //xmemDumpRange(requestedAddr, lenType);
#endif

    }
    void mmapHandler(int* clientId) {
      size_t lenMmap;
#ifdef DEBUG_UVA
      LOG("[server] get mmap request from client (%d)\n", *clientId);
#endif

      // get requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // get size variable's length (32 or 64 bits)
      size_t sizeOfLength = socket->takeWordF(clientId);

      // get length (how much mmap)
      socket->takeRangeF(&lenMmap, sizeOfLength, clientId);
#ifdef DEBUG_UVA
      LOG("[server] mmap length (how much mmap in byte): %d\n", lenMmap);
#endif

      void* allocAddr = xmemPagemap(requestedAddr, lenMmap, true);

      assert(allocAddr != NULL && "mmap alloc failed in server");

      socket->pushWordF(MMAP_REQ_ACK, clientId); // ACK
      socket->pushWordF(0, clientId); // ACK (0: normal, -1:abnormal)
      socket->sendQue(clientId);
    }
    void memsetHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get memset request from client (%d)\n", *clientId);
#endif
      void* requestedAddr = reinterpret_cast<void*>(socket->takeWordF(clientId));
      int value = socket->takeWordF(clientId);
      size_t num = socket->takeWordF(clientId);

#ifdef DEBUG_UVA
      LOG("[server] memset(%p, %d, %d)\n", requestedAddr, value, num);
#endif
      memset(requestedAddr, value, num);

      socket->pushWordF(MEMSET_REQ_ACK, clientId);
      socket->sendQue(clientId);
#ifdef DEBUG_UVA
      hexdump("memset", requestedAddr, num);
#endif
      //xmemDumpRange(requestedAddr, num);
    }
    void memcpyHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get memcpy request from client\n");
#endif
      int typeMemcpy = socket->takeWordF(clientId);
      if (typeMemcpy == 1) {
        void* dest = reinterpret_cast<void*>(socket->takeWordF(clientId));
        size_t num = socket->takeWordF(clientId);
        void* valueToStore = malloc(num);
        socket->takeRangeF(valueToStore, num, clientId);
#ifdef DEBUG_UVA
        //hexdump("server", valueToStore, num);
#endif


#ifdef DEBUG_UVA
        LOG("[server] memcpy(%p, , %d)\n", dest, num);
#endif
        //LOG("[server] below are src mem stat\n");
        //xmemDumpRange(src, num);
        memcpy(dest, valueToStore, num);

        socket->pushWordF(MEMCPY_REQ_ACK, clientId);
        socket->sendQue(clientId);
        //xmemDumpRange(dest, num);
#ifdef DEBUG_UVA
        //hexdump("memcpy dest", dest, num); 
#endif
      } else if (typeMemcpy == 2) {
        void* src = reinterpret_cast<void*>(socket->takeWordF(clientId));
        size_t num = socket->takeWordF(clientId);
        socket->pushRangeF(src, num);

        // don't need to do memcpy in server

        socket->sendQue(clientId);
      }
    }

    void globalSegfaultHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get GLOBAL_SEGFALUT_REQ from client (%d)\n", *clientId);
#endif

      void* ptNoConstBegin = reinterpret_cast<void*>(socket->takeWordF(clientId));
      void* ptNoConstEnd = reinterpret_cast<void*>(socket->takeWordF(clientId));

      uintptr_t target = (uintptr_t)(&ptNoConstBegin);
#ifdef DEBUG_UVA
      LOG("[server] TEST ptConstBegin (%p)\n", (void*)(*((uintptr_t *)target)));

      LOG("[server] send ack (%d)\n", GLOBAL_SEGFAULT_REQ_ACK);
#endif
      socket->pushWordF(GLOBAL_SEGFAULT_REQ_ACK, clientId); // ACK
      socket->pushRangeF((void*)(*((uintptr_t *)(uintptr_t)(&ptNoConstBegin))),
          (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin, clientId);
      //socket->pushRangeF((void*)(*((uintptr_t *)(uintptr_t)(&ptConstBegin))), (uintptr_t)ptConstEnd - (uintptr_t)ptConstBegin, clientId);
      socket->sendQue(clientId);
#ifdef DEBUG_UVA
      LOG("[server] GLOBAL_SEGFALUT_REQ process end (%d)\n", *clientId);
#endif
    }

    void globalInitCompleteHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get GLOBAL_INIT_COMPLETE_SIG from client (%d)\n", *clientId);
#endif
      if (isInitEnd) {
#ifdef DEBUG_UVA
        LOG("[server] already complete... somthing wrong..\n");
#endif
        return;
      }

      isInitEnd = true;
      for(auto &i : *RuntimeClientConnTb) {
        if(*(i.first) != *clientId) {
#ifdef DEBUG_UVA
          LOG("[server] send 'start permission' signal to client (%d)\n", *(i.first));
#endif
          socket->pushWordF(1, i.first);
          socket->sendQue(i.first);
        }
      }
      socket->pushWordF(GLOBAL_INIT_COMPLETE_SIG_ACK, clientId);
      socket->sendQue(clientId);
    }


    // XXX DEPRECATED : These two function may not be used.
    extern "C" void uva_server_load(void *addr, size_t len) {
#ifdef DEBUG_UVA
      LOG("[server] Load instr, addr %p, len %d\n", addr, len); 
#endif
    }


    extern "C" void uva_server_store(void *addr, size_t len, void *data) {
#ifdef DEBUG_UVA
      LOG("[server] Store instr, addr %p, len %d\n", addr, len); 
#endif
    }
  }
}

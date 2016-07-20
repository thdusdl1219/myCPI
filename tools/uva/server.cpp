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

#include "TimeUtil.h"
#include "uva_debug_eval.h"

#define HLRC
//#define DEBUG_UVA

#define TEST 1

using namespace corelab::XMemory;
namespace corelab {
  namespace UVA {

    static QSocket* socket;
    static bool isFirstUvaSync = true;
    pthread_t openThread; 
    pthread_mutex_t acquireLock =PTHREAD_MUTEX_INITIALIZER;

		static inline void* truncToPageAddr (void *addr) {
			return (void *)((XmemUintPtr)addr & XMEM_PAGE_MASK);
		}

    extern "C" void UVAServerInitialize() {
#ifdef DEBUG_UVA
      LOG("UVA manager(server) : initialize\n");
#endif
      RuntimeClientConnTb = new map<int *, QSocket *>(); 
      pageMap = new map<long, struct pageInfo*>();
      assert(!isInitEnd && "When server init, isInitEnd value should be false.");
      pthread_create(&openThread, NULL, ServerOpenRoutine, NULL);
    }

    static void resetServer() {
      if (RuntimeClientConnTb->empty()) {
        delete RuntimeClientConnTb;
        delete pageMap;
        RuntimeClientConnTb = new map<int *, QSocket *>(); 
        pageMap = new map<long, struct pageInfo*>();
        isInitEnd = false;
      }
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
      //fprintf(stderr, "system : Enter port to listen : ");
      //scanf("%s", port);
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
      int mode;
      int rval = 0; // for thread_exit

      while(true) {
//        pthread_mutex_lock(&mutex);
        socket->receiveQue(clientId);
#ifdef UVA_EVAL
        StopWatch watchHandle;
        watchHandle.start();
#endif
        
        mode = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
        LOG("[server] *** Receive message from client (id: %d, mode %d) ***\n", *clientId, mode);  
#endif

        switch(mode) {
          case THREAD_EXIT:
            //pthread_mutex_unlock(&mutex);
            RuntimeClientConnTb->erase(clientId);
#ifdef DEBUG_UVA
            LOG("[server] thread exit! now # of connected clients : (%d)\n\n", RuntimeClientConnTb->size());
#endif
            pthread_exit(&rval);
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
            // XXX: should handle HLRC version.
            memsetHandler(clientId);
            break;
          case MEMCPY_REQ:
#ifdef HLRC
            memcpyHandlerForHLRC(clientId);
#else
            memcpyHandler(clientId);
#endif
            break;
          case HEAP_SEGFAULT_REQ:
            heapSegfaultHandler(clientId);
            break;
          case GLOBAL_SEGFAULT_REQ:
            globalSegfaultHandler(clientId);
            break;
          case GLOBAL_INIT_COMPLETE_SIG:
            globalInitCompleteHandler(clientId);
            break;
          case INVALID_REQ:
            invalidHandler(clientId);
            break;
          case RELEASE_REQ:
            releaseHandler(clientId);
            break;
          case SYNC_REQ:
            syncHandler(clientId);
            break;
          default:
            assert(0 && "wrong request mode");
            break;
        }
        //pthread_mutex_unlock(&mutex); // TODO need acquire & release lock
/*#ifdef UVA_EVAL
        watchHandle.end();
        printf("HANDLE %lf mode (%d) %08x\n", watchHandle.diff(), mode, id);
#endif*/
      }
      return NULL;
    }

    void invalidHandler(int* clientId) {
      pthread_mutex_lock(&acquireLock);
      set<uint32_t> sendAddrSet;
      // find same cliendId in accessSet in pageInfo
      for(map<long, struct pageInfo*>::iterator it = pageMap->begin(); it != pageMap->end(); it++) {
        set<int>* my_var = it->second->accessS;
        if(my_var->find(*clientId) == my_var->end()) {
          uint32_t intAddr;
          memcpy(&intAddr, &(it->first), 4);
          sendAddrSet.insert(intAddr);
#ifdef DEBUG_UVA
          LOG("[server] add invalidation address (%p)(%d) for clientId (%d)\n", reinterpret_cast<void*>(it->first), intAddr, *clientId);
#endif
        }
      }
      socket->pushWord(INVALID_REQ_ACK, clientId); 
      //socket->pushWord(sizeof(void*), clientId); // send addressSize XXX support x64
      socket->pushWord(sendAddrSet.size(), clientId); // send addressNum 

      // send address
      //long* addressbuf = (long *) malloc(sizeof(void*));
      uint32_t *addressbuf = (uint32_t*) malloc(4);
      for(set<uint32_t>::iterator it = sendAddrSet.begin(); it != sendAddrSet.end(); it++) {
        //*addressbuf = *it * PAGE_SIZE;
        *addressbuf = *it;
        //socket->pushRange(addressbuf, sizeof(void*), clientId); 
        socket->pushRange(addressbuf, 4, clientId); 
      }
      socket->sendQue(clientId);
      free(addressbuf);

#ifdef DEBUG_UVA
      LOG("[server] send invalid Address");
#endif
    }

    /* @detail releaseHandler 
     *  1. take store logs (aka diff or changes) from releaser
     *  2. apply store logs into Home's corresponding pages
     */
    void releaseHandler(int *clientId) {
      int sizeStoreLogs;
      void *storeLogs;
//      socket->receiveQue(clientId);
      sizeStoreLogs = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
          LOG("[server] sizeStoreLogs : (%d)\n", sizeStoreLogs);
#endif
      storeLogs = malloc(sizeStoreLogs);
#ifdef DEBUG_UVA
          LOG("[server] StoreLogs address : (%p)\n", storeLogs);
#endif
      socket->takeRangeF(storeLogs, sizeStoreLogs, clientId);
      
#if UINTPTR_MAX == 0xffffffff
      /* 32-bit */
      uint32_t intAddrOfStoreLogs;
      memcpy(&intAddrOfStoreLogs, &storeLogs, 4); 
      uint32_t current = intAddrOfStoreLogs;
#elif UINTPTR_MAX == 0xffffffffffffffff
      /* 64-bit */
      uint64_t intAddrOfStoreLogs;
      memcpy(&intAddrOfStoreLogs, &storeLogs, 8); 
      uint64_t current = intAddrOfStoreLogs;
#else
      /* hmm ... */
      assert(0);
#endif
      uint32_t size;
      void *data;
      void **addr;
      addr = (void **)malloc(4);
      while (current != intAddrOfStoreLogs + sizeStoreLogs) {
        memcpy(&size, reinterpret_cast<void*>(current), 4);
        data = malloc(size);
        memcpy(data, reinterpret_cast<void*>(current+4), size);
        memset(addr, 0, 8);
        memcpy(addr, reinterpret_cast<void*>(current+4+size), 4);
        
#ifdef DEBUG_UVA
        LOG("[server] in while | curStoreLog (size:%d, addr:%p, data:%x)\n", size, *addr, *(int*)data);
#endif        
        memcpy(*addr, data, size);
#ifdef DEBUG_UVA
        hexdump("release", *addr, size);
#endif
        free(data);
        current = current + 8 + size;
      } // while END
      free(addr);
      free(storeLogs);
      pthread_mutex_unlock(&acquireLock);
    }

    void syncHandler(int *clientId) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      pthread_mutex_lock(&acquireLock);
#ifdef DEBUG_UVA
      LOG("[server] syncHandler START\n");
#endif
      int sizeStoreLogs;
      void *storeLogs;
      sizeStoreLogs = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
      LOG("[server] sizeStoreLogs : (%d)\n", sizeStoreLogs);
#endif
      set<uint32_t> sendAddrSet;
      // find same cliendId in accessSet in pageInfo
      for(map<long, struct pageInfo*>::iterator it = pageMap->begin(); it != pageMap->end(); it++) {
        set<int>* my_var = it->second->accessS;
        if(my_var->find(*clientId) == my_var->end()) {
          uint32_t intAddr;
          memcpy(&intAddr, &(it->first), 4);
          sendAddrSet.insert(intAddr);
#ifdef DEBUG_UVA
          LOG("[server] add invalidation address (%p)(%d) for clientId (%d)\n", reinterpret_cast<void*>(it->first), intAddr, *clientId);
#endif
        }
      }
      if (sizeStoreLogs != 0) {
        storeLogs = malloc(sizeStoreLogs);
        socket->takeRangeF(storeLogs, sizeStoreLogs, clientId);
#if UINTPTR_MAX == 0xffffffff
        /* 32-bit */
        uint32_t intAddrOfStoreLogs;
        memcpy(&intAddrOfStoreLogs, &storeLogs, 4); 
        uint32_t current = intAddrOfStoreLogs;
#elif UINTPTR_MAX == 0xffffffffffffffff
        /* 64-bit */
        uint64_t intAddrOfStoreLogs;
        memcpy(&intAddrOfStoreLogs, &storeLogs, 8); 
        uint64_t current = intAddrOfStoreLogs;
#else
        /* hmm ... */
        assert(0);
#endif
        uint32_t size;
        void *data;
        void **addr;
        addr = (void **)malloc(4);
        while (current != intAddrOfStoreLogs + sizeStoreLogs) {
          memcpy(&size, reinterpret_cast<void*>(current), 4);
          data = malloc(size);
          memcpy(data, reinterpret_cast<void*>(current+4), size);
          memset(addr, 0, 8);
          memcpy(addr, reinterpret_cast<void*>(current+4+size), 4);

#ifdef DEBUG_UVA
          LOG("[server] in while | curStoreLog (size:%d, addr:%p, data:%x)\n", size, *addr, *(int*)data);
#endif
          memcpy(*addr, data, size);
          //pageMap[(long)(truncToPageAddr(*addr))]->accessS->insert(*clientId);
          struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(*addr))];
          if (pageInfo != NULL) {
            pageInfo->accessS->clear();
            pageInfo->accessS->insert(*clientId);
#ifdef DEBUG_UVA
            LOG("[server] page (%p)'s accessSet is updated, clientId (%d)\n", truncToPageAddr(*addr), *clientId);
#endif
          } else {
            assert(0);
          }
#ifdef DEBUG_UVA
          //hexdump("release", *addr, size);
#endif
          free(data);
          current = current + 8 + size;
        } // while END
        free(addr);
        free(storeLogs);
      }

      //socket->pushWord(sizeof(void*), clientId); // send addressSize XXX support x64
      //socket->pushWord(4, clientId); // send addressSize XXX support x64
      socket->pushWord(sendAddrSet.size(), clientId); // send addressNum 

      // send address
      //long* addressbuf = (long *) malloc(sizeof(void*));
      uint32_t *addressbuf = (uint32_t*) malloc(4);
      for(set<uint32_t>::iterator it = sendAddrSet.begin(); it != sendAddrSet.end(); it++) {
        //*addressbuf = *it * PAGE_SIZE;
        *addressbuf = *it;
        //socket->pushRange(addressbuf, sizeof(void*), clientId); 
        socket->pushRange(addressbuf, 4, clientId); 
      }
      socket->sendQue(clientId);
      free(addressbuf);

#ifdef DEBUG_UVA
      LOG("[server] syncHandler END\n\n");
#endif
      pthread_mutex_unlock(&acquireLock);
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval-server.txt", "a");
      fprintf(fp, "SYNC %lf\n", watch.diff());
      fclose(fp);
#endif
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
      uint32_t current;
      uint32_t lastPageAddr;
      void *current_ = truncToPageAddr(allocAddr);
      memcpy(&current, &current_, 4);
      void *lastPageAddr_ = truncToPageAddr((void*)(current + lenbuf - 1));
      memcpy(&lastPageAddr, &lastPageAddr_, 4);
#ifdef DEBUG_UVA
      LOG("[server] current (%p) lastPageAddr (%p)\n", reinterpret_cast<void*>(current), reinterpret_cast<void*>(lastPageAddr));
#endif
      while(current <= lastPageAddr) {
        struct pageInfo* newPageInfo = new pageInfo();
        newPageInfo->accessS->insert(*clientId);
        //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
        (*pageMap)[(long)current] = newPageInfo;
#ifdef DEBUG_UVA
        LOG("[server] current (%p) is added into PageMap\n", reinterpret_cast<void*>(current));
#endif
        //assert(allocAddr != NULL && "mmap alloc failed in server");
        current += PAGE_SIZE;
      }

      // memory operation end
      socket->pushWordF(HEAP_ALLOC_REQ_ACK, clientId);
      socket->pushWordF(sizeof(allocAddr), clientId);
      socket->pushRangeF(&allocAddr, sizeof(allocAddr), clientId);
      socket->sendQue(clientId);
      return;
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
      return;
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
      free(valueToStore);
      // send ack
      socket->pushWordF(STORE_REQ_ACK, clientId); // ACK
      socket->pushWordF(0, clientId); // ACK ( 0: normal, -1: abnormal ) FIXME useless
      socket->sendQue(clientId);

#ifdef DEBUG_UVA
      // test
      hexdump("store", requestedAddr, lenType);
      //xmemDumpRange(requestedAddr, lenType);
#endif
      return;
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
#ifdef DEBUG_UVA
      LOG("[server] allocAddr : %p\n", allocAddr);
#endif

      uint32_t current;
      uint32_t lastPageAddr;
      void *current_ = truncToPageAddr(requestedAddr);
      memcpy(&current, &current_, 4);
      void *lastPageAddr_ = truncToPageAddr((void*)(current + sizeOfLength - 1));
      memcpy(&lastPageAddr, &lastPageAddr_, 4);
#ifdef DEBUG_UVA
      LOG("[server] current (%p) lastPageAddr (%p)\n", reinterpret_cast<void*>(current), reinterpret_cast<void*>(lastPageAddr));
#endif
      while(current <= lastPageAddr) {
        struct pageInfo* newPageInfo = new pageInfo();
        newPageInfo->accessS->insert(*clientId);
        //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
        (*pageMap)[(long)current] = newPageInfo;
#ifdef DEBUG_UVA
        LOG("[server] current (%p) is added into PageMap\n", reinterpret_cast<void*>(current));
#endif
        assert(allocAddr != NULL && "mmap alloc failed in server");
        current += PAGE_SIZE;
      }

      socket->pushWordF(MMAP_REQ_ACK, clientId); // ACK
      socket->pushWordF(0, clientId); // ACK (0: normal, -1:abnormal)
      socket->sendQue(clientId);
      return;
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
      return;
    }
    void memcpyHandler(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] get memcpy request from client\n");
#endif
      int typeMemcpy = socket->takeWordF(clientId);
      if (typeMemcpy == 1) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 1 | dest is in UVA\n");
#endif
        void* dest = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy dest addr (%p)\n", dest);
#endif
        size_t num = socket->takeWordF(clientId);
        void* valueToStore = malloc(num);
        socket->takeRangeF(valueToStore, num, clientId);
#ifdef DEBUG_UVA
        //hexdump("server", valueToStore, num);
        LOG("[server] memcpy(%p, , %d)\n", dest, num);
#endif
        //LOG("[server] below are src mem stat\n");
        //xmemDumpRange(src, num);
        memcpy(dest, valueToStore, num);
        free(valueToStore);

        socket->pushWordF(MEMCPY_REQ_ACK, clientId);
        socket->sendQue(clientId);
#ifdef DEBUG_UVA
        //hexdump("memcpy dest", dest, num); 
#endif
      } else if (typeMemcpy == 2) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 2 | src is in UVA, dest isn't in UVA\n");
#endif
        void* src = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy src addr (%p)\n", src);
#endif
        size_t num = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy num (%d)\n", num);
#endif
        socket->pushRangeF(src, num, clientId);
        // don't need to do memcpy in server
        socket->sendQue(clientId);
      }
      return;
    }

    void memcpyHandlerForHLRC(int* clientId) {
#ifdef DEBUG_UVA
      LOG("[server] HLRC get memcpy request from client\n");
#endif
      int typeMemcpy = socket->takeWordF(clientId);
      if (typeMemcpy == 1) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 1 | dest is in UVA | SHOULD NOT BE!!!!\n");
#endif
        assert(0);
      } else if (typeMemcpy == 2) {
#ifdef DEBUG_UVA
        LOG("[server] HLRC typeMemcpy 2 | src is in UVA, dest isn't in UVA\n");
#endif
        void* src = reinterpret_cast<void*>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
        LOG("[server] HLRC requested memcpy src addr (%p)\n", src);
#endif
        size_t num = socket->takeWordF(clientId);
#ifdef DEBUG_UVA
        LOG("[server] HLRC requested memcpy num (%d)\n", num);
#endif
        uint32_t current;
        uint32_t lastPageAddr;
        void *current_ = truncToPageAddr(src);
        memcpy(&current, &current_, 4);
        void *lastPageAddr_ = truncToPageAddr((void*)(current + num - 1));
        memcpy(&lastPageAddr, &lastPageAddr_, 4);
#ifdef DEBUG_UVA
        LOG("[server] current (%p) lastPageAddr (%p)\n", reinterpret_cast<void*>(current), reinterpret_cast<void*>(lastPageAddr));
#endif
        while(current <= lastPageAddr) {
          struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(current_))];
          if (pageInfo != NULL) {
          pageInfo->accessS->insert(*clientId);
          //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
#ifdef DEBUG_UVA
          LOG("[server] client (%d) is added into (%p) in PageMap\n", *clientId, reinterpret_cast<void*>(current));
#endif
          } else {
            assert(0);
          }
          current += PAGE_SIZE;
        }
        socket->pushRangeF(src, num, clientId);
        // don't need to do memcpy in server
        socket->sendQue(clientId);
      }
      return;
    }
    void heapSegfaultHandler(int* clientId) {
      void **fault_heap_addr = reinterpret_cast<void**>(socket->takeWordF(clientId));
#ifdef DEBUG_UVA
      LOG("[server] get HEAP_SEGFAULT_REQ from client (%d) on (%p)\n", *clientId, fault_heap_addr);
#endif
      socket->pushRangeF(truncToPageAddr(fault_heap_addr), 0x1000, clientId);
      
      /* XXX: Below are for optimization ... not sure XXX */
      void *trunc = truncToPageAddr(fault_heap_addr);
#ifdef DEBUG_UVA
      LOG("[server] fault page addr : %p(%lu)\n", trunc, (long)trunc);
#endif
      struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(fault_heap_addr))];
      if (pageInfo != NULL) {
        //pageInfo->accessS->clear();
        pageInfo->accessS->insert(*clientId);
#ifdef DEBUG_UVA
        LOG("[server] page (%p)'s accessSet is updated, clientId (%d)\n", truncToPageAddr(fault_heap_addr), *clientId);
#endif
      } else {
        assert(0);
      }
      socket->sendQue(clientId);
      return;
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
      
      /* XXX: Below are for optimization ... not sure XXX */
      /* XXX: only one page? No -> should be fixed */
      struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(ptNoConstBegin))];
      if (pageInfo != NULL) {
        //pageInfo->accessS->clear();
        pageInfo->accessS->insert(*clientId);
#ifdef DEBUG_UVA
        LOG("[server] page (%p)'s accessSet is updated, clientId (%d)\n", truncToPageAddr(ptNoConstBegin), *clientId);
#endif
      } else {
        assert(0);
      }

      socket->sendQue(clientId);
#ifdef DEBUG_UVA
      LOG("[server] GLOBAL_SEGFALUT_REQ process end (%d)\n", *clientId);
#endif
      return;
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
      return;
    }



    // XXX XXX XXX XXX XXX 
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

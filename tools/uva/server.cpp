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

    static CommManager *comm;
    //pthread_t openThread; 
    //pthread_mutex_t acquireLock = PTHREAD_MUTEX_INITIALIZER;

		static inline void* truncToPageAddr (void *addr) {
			return (void *)((XmemUintPtr)addr & XMEM_PAGE_MASK);
		}

    extern "C" void UVAServerCallbackSetter(CommManager *comm) {
      TAG tag;

      tag = NEWFACE_HANDLER;
      comm->setCallback(tag, newfaceHandler);
      tag = MALLOC_HANDLER;
      comm->setCallback(tag, heapAllocHandler);
      tag = LOAD_HANDLER;
      comm->setCallback(tag, loadHandler);
      tag = STORE_HANDLER;
      comm->setCallback(tag, storeHandler);
      //tag = STORE_HLRC_HANDLER;
      //comm->setCallback(tag, storeHandlerForHLRC); // XXX
      tag = ACQUIRE_HANDLER;
      comm->setCallback(tag, acquireHandler);
      tag = RELEASE_HANDLER;
      comm->setCallback(tag, releaseHandler);
      tag = SYNC_HANDLER;
      comm->setCallback(tag, syncHandler);
      tag = MMAP_HANDLER;
      comm->setCallback(tag, mmapHandler);
      tag = MEMSET_HANDLER;
      comm->setCallback(tag, memsetHandler);
      tag = MEMCPY_HANDLER;
      comm->setCallback(tag, memcpyHandler);
      tag = MEMCPY_HLRC_HANDLER;
      comm->setCallback(tag, memcpyHandlerForHLRC);
      tag = GLOBAL_SEGFAULT_HANDLER;
      comm->setCallback(tag, globalSegfaultHandler);
      tag = HEAP_SEGFAULT_HANDLER;
      comm->setCallback(tag, heapSegfaultHandler);
      tag = GLOBAL_INIT_COMPLETE_HANDLER;
      comm->setCallback(tag, globalInitCompleteHandler);

    }

    extern "C" void UVAServerInitializer(CommManager *comm_) {
#ifdef DEBUG_UVA
      LOG("UVA manager(server) : initialize\n");
#endif
      //RuntimeClientConnTb = new map<int *, QSocket *>(); 
      RuntimeClientConnTb = new vector<uint32_t>();
      pageMap = new map<long, struct pageInfo*>();
      assert(!isInitEnd && "When server init, isInitEnd value should be false.");

      comm = comm_;
      //pthread_create(&openThread, NULL, ServerOpenRoutine, NULL);
    }
/*
    static void resetServer() {
      if (RuntimeClientConnTb->empty()) {
        delete RuntimeClientConnTb;
        delete pageMap;
        RuntimeClientConnTb = new map<int *, QSocket *>(); 
        pageMap = new map<long, struct pageInfo*>();
        isInitEnd = false;
      }
    }
*/
    extern "C" void UVAServerFinalizer() {
      //pthread_join(openThread, NULL);
      delete RuntimeClientConnTb;
      delete pageMap;
    }
#if 0
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
#endif

    // XXX XXX XXX
    void newfaceHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[SERVER] New Face is comming !! (srcid:%d)\n", srcid);
#endif
      if (std::find(RuntimeClientConnTb->begin(), RuntimeClientConnTb->end(), srcid) != RuntimeClientConnTb->end()) {
        assert(0 && "[SERVER] YOU ARE NOT A NEW FACE !!\n");
      } else { 
#ifdef DEBUG_UVA
        LOG("[SERVER] New Face (%d) is going in RuntimeClientConnTb\n", srcid);
#endif
        RuntimeClientConnTb->push_back(srcid);
        if (isInitEnd) {
#ifdef DEBUG_UVA
          LOG("[SERVER] Oh.. you are late (This client comes in after glb init finished\n");
#endif
          comm->pushWord(BLOCKING, 1, srcid); // send permission.
          comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
          LOG("[SERVER] newfaceHandler END (successfully sends start permission) (srcid:%d)\n", srcid);
#endif
        } else {
#ifdef DEBUG_UVA
          LOG("[SERVER] newfaceHandler END (couldn't sends start permission) (srcid:%d)\n", srcid);
#endif
        }
      }
    }

    void acquireHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] acquireHandler START (srcid:%d)", srcid);
#endif
      //pthread_mutex_lock(&acquireLock);
      set<uint32_t> sendAddrSet;
      // find same cliendId in accessSet in pageInfo
      for(map<long, struct pageInfo*>::iterator it = pageMap->begin(); it != pageMap->end(); it++) {
        set<int>* my_var = it->second->accessS;
        if(my_var->find(srcid) == my_var->end()) {
          uint32_t intAddr;
          memcpy(&intAddr, &(it->first), 4);
          sendAddrSet.insert(intAddr);
#ifdef DEBUG_UVA
          LOG("[server] add invalidation address (%p)(%d) for srcid (%d)\n", reinterpret_cast<void*>(it->first), intAddr, srcid);
#endif
        }
      }
      //comm->pushWord(BLOCKING, ACQUIRE_REQ_ACK, srcid); 
      //socket->pushWord(sizeof(void*), clientId); // send addressSize XXX support x64
      comm->pushWord(BLOCKING, sendAddrSet.size(), srcid); // send addressNum 

      // send address
      //long* addressbuf = (long *) malloc(sizeof(void*));
      uint32_t *addressbuf = (uint32_t*) malloc(4);
      for(set<uint32_t>::iterator it = sendAddrSet.begin(); it != sendAddrSet.end(); it++) {
        //*addressbuf = *it * PAGE_SIZE;
        *addressbuf = *it;
        //socket->pushRange(addressbuf, sizeof(void*), clientId); 
        comm->pushRange(BLOCKING, addressbuf, 4, srcid); 
      }
      comm->sendQue(BLOCKING, srcid);
      free(addressbuf);

#ifdef DEBUG_UVA
      LOG("[server] acquireHandler END (srcid:%d)", srcid);
#endif
    }

    /* @detail releaseHandler 
     *  1. take store logs (aka diff or changes) from releaser
     *  2. apply store logs into Home's corresponding pages
     */
    void releaseHandler(void *data_, uint32_t size_, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] releaseHandler END (srcid:%d)", srcid);
#endif
      int sizeStoreLogs;
      void *storeLogs;
//      socket->receiveQue(clientId);
      sizeStoreLogs = *(int*)data_;
#ifdef DEBUG_UVA
      LOG("[server] sizeStoreLogs : (%d)\n", sizeStoreLogs);
#endif
      storeLogs = malloc(sizeStoreLogs);
#ifdef DEBUG_UVA
      LOG("[server] StoreLogs address : (%p)\n", storeLogs);
#endif
      //socket->takeRangeF(storeLogs, sizeStoreLogs, clientId);
      memcpy(storeLogs, (char*)data_ + 4, sizeStoreLogs);
      
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
      //pthread_mutex_unlock(&acquireLock);
    }

    /******************************/
    /******** SYNC HANDLER ********/
    /******************************/
    void syncHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      //pthread_mutex_lock(&acquireLock);
#ifdef DEBUG_UVA
      LOG("[server] syncHandler START (srcid:%d)\n", srcid);
#endif
      int sizeStoreLogs;
      void *storeLogs;
      sizeStoreLogs = *(int*)data_;
#ifdef DEBUG_UVA
      LOG("[server] sizeStoreLogs : (%d)\n", sizeStoreLogs);
#endif
      set<uint32_t> sendAddrSet;
      // find same cliendId in accessSet in pageInfo
      for(map<long, struct pageInfo*>::iterator it = pageMap->begin(); it != pageMap->end(); it++) {
        set<int>* my_var = it->second->accessS;
        if(my_var->find(srcid) == my_var->end()) {
          uint32_t intAddr;
          memcpy(&intAddr, &(it->first), 4);
          sendAddrSet.insert(intAddr);
#ifdef DEBUG_UVA
          LOG("[server] add invalidation address (%p)(%d) for srcid (%d)\n", reinterpret_cast<void*>(it->first), intAddr, srcid);
#endif
        }
      }
      if (sizeStoreLogs != 0) {
        storeLogs = malloc(sizeStoreLogs);
        //socket->takeRangeF(storeLogs, sizeStoreLogs, clientId);
        memcpy(storeLogs, (char*)data_ + 4, sizeStoreLogs); // XXX
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
            pageInfo->accessS->insert(srcid);
#ifdef DEBUG_UVA
            LOG("[server] page (%p)'s accessSet is updated, srcid (%d)\n", truncToPageAddr(*addr), srcid);
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
      comm->pushWord(BLOCKING, sendAddrSet.size(), srcid); // send addressNum 

      // send address
      //long* addressbuf = (long *) malloc(sizeof(void*));
      uint32_t *addressbuf = (uint32_t*) malloc(4);
      for(set<uint32_t>::iterator it = sendAddrSet.begin(); it != sendAddrSet.end(); it++) {
        //*addressbuf = *it * PAGE_SIZE;
        *addressbuf = *it;
        //socket->pushRange(addressbuf, sizeof(void*), clientId); 
        comm->pushRange(BLOCKING, addressbuf, 4, srcid); 
      }
      comm->sendQue(BLOCKING, srcid);
      free(addressbuf);

#ifdef DEBUG_UVA
      LOG("[server] syncHandler END (srcid:%d)\n\n", srcid);
#endif
      //pthread_mutex_unlock(&acquireLock);
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval-server.txt", "a");
      fprintf(fp, "SYNC %lf\n", watch.diff());
      fclose(fp);
#endif
    }

    void heapAllocHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[SERVER] heapAllocHandler START (srcid:%d)\n", srcid);
#endif
      int lenbuf;
      //int datalen = socket->takeWordF(clientId);
      int datalen = *(int*)data_;
#ifdef DEBUG_UVA
      LOG("[server] (datalen) : (%d)\n", datalen);
#endif
      //socket->takeRange(&lenbuf, datalen, clientId);
      memcpy(&lenbuf, (char*)data_ + 4, datalen);
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
        newPageInfo->accessS->insert(srcid);
        //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
        (*pageMap)[(long)current] = newPageInfo;
#ifdef DEBUG_UVA
        //LOG("[server] current (%p) is added into PageMap\n", reinterpret_cast<void*>(current));
#endif
        //assert(allocAddr != NULL && "mmap alloc failed in server");
        current += PAGE_SIZE;
      }

      // memory operation end
      uint32_t sizeOfAllocAddr = (uint32_t)sizeof(allocAddr);
      //comm->pushWord(BLOCKING, HEAP_ALLOC_REQ_ACK, srcid);
      comm->pushWord(BLOCKING, sizeOfAllocAddr, srcid);
      comm->pushRange(BLOCKING, &allocAddr, sizeof(allocAddr), srcid);
      comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[SERVER] heapAllocHandler END (srcid:%d)\n", srcid);
#endif
      return;
    }

    void loadHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] get Load request from client (id: %d)\n", srcid);
#endif

      // receive type length (how much load in byte)
      size_t lenType = *(size_t*)data_;
#ifdef DEBUG_UVA
      LOG("[server] type length (how much): %d\n", lenType);
#endif

      // receive requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // send ack with value (what to load)
      comm->pushWord(BLOCKING, LOAD_REQ_ACK, srcid);
      comm->pushWord(BLOCKING, lenType, srcid);
      comm->pushRange(BLOCKING, requestedAddr, lenType, srcid);
#ifdef DEBUG_UVA
      LOG("[server] TEST loaded value (what): %d\n", *((int*)requestedAddr));
#endif
      comm->sendQue(BLOCKING, srcid);
      return;
    }
    void storeHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] get store request from client (%d)\n", srcid);
#endif

      // get type length (how much store in byte)
      size_t lenType = *(int*)data_;
#ifdef DEBUG_UVA
      LOG("[server] type length (how much store in byte): %d\n", lenType);
#endif

      // get requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // get value which client want to store (what to store)
      void* valueToStore = malloc(lenType);
      //socket->takeRangeF(valueToStore, lenType, clientId);
      memcpy(valueToStore, (char*)data_ + 8, lenType);
#ifdef DEBUG_UVA
      LOG("[server] TEST stored value (what): %d\n", *((int*)valueToStore));
#endif

      // store value in UVA address.
      memcpy(requestedAddr, valueToStore, lenType);
      free(valueToStore);
      // send ack
      comm->pushWord(BLOCKING, STORE_REQ_ACK, srcid); // ACK
      comm->pushWord(BLOCKING, 0, srcid); // ACK ( 0: normal, -1: abnormal ) FIXME useless
      comm->sendQue(BLOCKING, srcid);

#ifdef DEBUG_UVA
      // test
      hexdump("store", requestedAddr, lenType);
      //xmemDumpRange(requestedAddr, lenType);
#endif
      return;
    }
    void mmapHandler(void *data_, uint32_t size, uint32_t srcid) {
      size_t lenMmap;
#ifdef DEBUG_UVA
      LOG("[server] mmapHandler START (srcid:%d)\n", srcid);
#endif

      // get requested addr (where)
      void* requestedAddr = reinterpret_cast<void*>(*(int*)data_);
#ifdef DEBUG_UVA
      LOG("[server] requestedAddr (where): (%p)\n", requestedAddr);
#endif

      // get size variable's length (32 or 64 bits)
      uint32_t sizeOfLength = *(uint32_t*)((char*)data_ + 4);

      // get length (how much mmap)
      //socket->takeRangeF(&lenMmap, sizeOfLength, clientId);
      memcpy(&lenMmap, (char*)data_ + 8, (size_t)sizeOfLength);
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
        newPageInfo->accessS->insert(srcid);
        //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
        (*pageMap)[(long)current] = newPageInfo;
#ifdef DEBUG_UVA
        LOG("[server] current (%p) is added into PageMap\n", reinterpret_cast<void*>(current));
#endif
        assert(allocAddr != NULL && "mmap alloc failed in server");
        current += PAGE_SIZE;
      }

      //comm->pushWord(BLOCKING, MMAP_REQ_ACK, srcid); // ACK
      //comm->pushWord(BLOCKING, 0, srcid); // ACK (0: normal, -1:abnormal)
      //comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[server] mmapHandler END (srcid:%d)\n", srcid);
#endif
      return;
    }
    void memsetHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] memsetHandler START (srcid:%d)\n", srcid);
#endif
      void* requestedAddr = reinterpret_cast<void*>(*(int*)data_);
      int value = *(int*)((char*)data_ + 4);;
      size_t num = *(size_t*)((char*)data_ + 8);

#ifdef DEBUG_UVA
      LOG("[server] memset(%p, %d, %d)\n", requestedAddr, value, num);
#endif
      memset(requestedAddr, value, num);

      comm->pushWord(BLOCKING, MEMSET_REQ_ACK, srcid);
      comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      hexdump("memset", requestedAddr, num);
#endif
      //xmemDumpRange(requestedAddr, num);
#ifdef DEBUG_UVA
      LOG("[server] memsetHandler END (srcid:%d)\n", srcid);
#endif
      return;
    }
    void memcpyHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] memcpyHandler START (srcid:%d)\n", srcid);
#endif
      int typeMemcpy = *(int*)data_;
      if (typeMemcpy == 1) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 1 | dest is in UVA\n");
#endif
        void* dest = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy dest addr (%p)\n", dest);
#endif
        size_t num = *(size_t*)((char*)data_ + 8);
        void* valueToStore = malloc(num);
        //socket->takeRangeF(valueToStore, num, clientId);
        memcpy(valueToStore, (char*)data_ + 12, num);
#ifdef DEBUG_UVA
        //hexdump("server", valueToStore, num);
        LOG("[server] memcpy(%p, , %d)\n", dest, num);
#endif
        //LOG("[server] below are src mem stat\n");
        //xmemDumpRange(src, num);
        memcpy(dest, valueToStore, num);
        free(valueToStore);

        comm->pushWord(BLOCKING, MEMCPY_REQ_ACK, srcid);
        comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
        //hexdump("memcpy dest", dest, num); 
#endif
      } else if (typeMemcpy == 2) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 2 | src is in UVA, dest isn't in UVA\n");
#endif
        void* src = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy src addr (%p)\n", src);
#endif
        size_t num = *(size_t*)((char*)data_ + 8);
#ifdef DEBUG_UVA
        LOG("[server] requested memcpy num (%d)\n", num);
#endif
        comm->pushRange(BLOCKING, src, num, srcid);
        // don't need to do memcpy in server
        comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[server] memcpyHandler END (srcid:%d)\n", srcid);
#endif
      }
      return;
    }

    void memcpyHandlerForHLRC(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] HLRC-MEMCPY memcpyHandlerForHLRC START (srcid:%d)\n", srcid);
#endif
      int typeMemcpy = *(int*)data_;
      if (typeMemcpy == 1) {
#ifdef DEBUG_UVA
        LOG("[server] typeMemcpy 1 | dest is in UVA | SHOULD NOT BE!!!!\n");
#endif
        assert(0);
      } else if (typeMemcpy == 2) {
#ifdef DEBUG_UVA
        LOG("[server] HLRC typeMemcpy 2 | src is in UVA, dest isn't in UVA\n");
#endif
        void* src = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));
#ifdef DEBUG_UVA
        LOG("[server] HLRC requested memcpy src addr (%p)\n", src);
#endif
        size_t num = *(size_t*)((char*)data_ + 8);
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
          pageInfo->accessS->insert(srcid);
          //pageMap->insert(map<long, struct pageInfo*>::value_type((long)allocAddr / PAGE_SIZE, newPageInfo));
#ifdef DEBUG_UVA
          LOG("[server] client (%d) is added into (%p) in PageMap\n", srcid, reinterpret_cast<void*>(current));
#endif
          } else {
            assert(0);
          }
          current += PAGE_SIZE;
        }
        comm->pushRange(BLOCKING, src, num, srcid);
        // don't need to do memcpy in server
        comm->sendQue(BLOCKING, srcid);
      }
#ifdef DEBUG_UVA
      LOG("[server] HLRC-MEMCPY memcpyHandlerForHLRC END (srcid:%d)\n", srcid);
#endif
      return;
    }
    void heapSegfaultHandler(void *data_, uint32_t size, uint32_t srcid) {
      void **fault_heap_addr = reinterpret_cast<void**>(*(int*)data_);
#ifdef DEBUG_UVA
      LOG("[server] get HEAP_SEGFAULT_REQ from client (%d) on (%p)\n", srcid, fault_heap_addr);
#endif
      comm->pushRange(BLOCKING, truncToPageAddr(fault_heap_addr), 0x1000, srcid);
      
      /* XXX: Below are for optimization ... not sure XXX */
      void *trunc = truncToPageAddr(fault_heap_addr);
#ifdef DEBUG_UVA
      LOG("[server] fault page addr : %p(%lu)\n", trunc, (long)trunc);
#endif
      struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(fault_heap_addr))];
      if (pageInfo != NULL) {
        //pageInfo->accessS->clear();
        pageInfo->accessS->insert(srcid);
#ifdef DEBUG_UVA
        LOG("[server] page (%p)'s accessSet is updated, srcid (%d)\n", truncToPageAddr(fault_heap_addr), srcid);
#endif
      } else {
        assert(0);
      }
      comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[server] heapSegfaultHandler END ** client (%d)'s fault on (%p) **\n", srcid, fault_heap_addr);
#endif
      return;
    }
    
    void globalSegfaultHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] get GLOBAL_SEGFALUT_REQ from client (%d)\n", srcid);
#endif

      void* ptNoConstBegin = reinterpret_cast<void*>(*(int*)data_);
      void* ptNoConstEnd = reinterpret_cast<void*>(*(int*)((char*)data_ + 4));

      uintptr_t target = (uintptr_t)(&ptNoConstBegin);
#ifdef DEBUG_UVA
      LOG("[server] TEST ptConstBegin (%p)\n", (void*)(*((uintptr_t *)target)));

      LOG("[server] send ack (%d)\n", GLOBAL_SEGFAULT_REQ_ACK);
#endif
      comm->pushWord(BLOCKING, GLOBAL_SEGFAULT_REQ_ACK, srcid); // ACK
      comm->pushRange(BLOCKING, (void*)(*((uintptr_t *)(uintptr_t)(&ptNoConstBegin))),
          (uintptr_t)ptNoConstEnd - (uintptr_t)ptNoConstBegin, srcid);
      //socket->pushRangeF((void*)(*((uintptr_t *)(uintptr_t)(&ptConstBegin))), (uintptr_t)ptConstEnd - (uintptr_t)ptConstBegin, clientId);
      
      /* XXX: Below are for optimization ... not sure XXX */
      /* XXX: only one page? No -> should be fixed */
      struct pageInfo *pageInfo = (*pageMap)[(long)(truncToPageAddr(ptNoConstBegin))];
      if (pageInfo != NULL) {
        //pageInfo->accessS->clear();
        pageInfo->accessS->insert(srcid);
#ifdef DEBUG_UVA
        LOG("[server] page (%p)'s accessSet is updated, clientId (%d)\n", truncToPageAddr(ptNoConstBegin), srcid);
#endif
      } else {
        assert(0);
      }

      comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[server] GLOBAL_SEGFALUT_REQ process end (%d)\n", srcid);
#endif
      return;
    }

    void globalInitCompleteHandler(void *data_, uint32_t size, uint32_t srcid) {
#ifdef DEBUG_UVA
      LOG("[server] Global Init Complete Signal is comming !! (srcid:%d)\n", srcid);
#endif
      if (isInitEnd) {
        assert(0 && "[server] already complete ... !? what did you do ?");
        return;
      }

      isInitEnd = true;
      RuntimeClientConnTb->push_back(srcid);
      for(auto &i : *RuntimeClientConnTb) {
        if(i != srcid) {
#ifdef DEBUG_UVA
          LOG("[server] send 'start permission' signal to client (%d)\n", i);
#endif
          comm->pushWord(BLOCKING, 1, i);
          comm->sendQue(BLOCKING, i);
        }
      }
      comm->pushWord(BLOCKING, GLOBAL_INIT_COMPLETE_SIG_ACK, srcid);
      comm->sendQue(BLOCKING, srcid);
#ifdef DEBUG_UVA
      LOG("[server] globalInitCompleteHandler END (srcid:%d)\n", srcid);
#endif
      return;
    }


#if 0
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
#endif

  }
}

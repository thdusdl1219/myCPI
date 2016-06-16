/***
 * uva_manager.cpp: UVA Manager
 *
 * Runtime UVA manager
 * written by: gwangmu
 *
 * **/

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <inttypes.h>
#include <vector>
#include <sys/mman.h>
#include <stdint.h>

#include "uva_manager.h"
#include "packet_header.h"
#include "pageset.h"
#include "xmem_alloc.h"
#include "xmem_info.h"
#include "mm.h" // XXX hmm.... BONGJUN
#include "debug.h"
#include "log.h"
#include "hexdump.h"
#include "uva_macro.h"

#include "TimeUtil.h"
//#define DEBUG_UVA
#define UVA_EVAL

#ifdef OVERHEAD_TEST
#include "overhead.h"
#endif

#ifdef INFLATE_READ
  #ifdef DEFLATE_WRITE
    #include "zlib/zlib.h"
  #endif
    #include "zlib/zlib.h"
#else
  #ifdef DEFLATE_WRITE
    #include "zlib/zlib.h"
  #endif
#endif

using namespace std;

namespace corelab {
	namespace UVA {
		static const unsigned MEMORY_TRANSFER_UNIT = 4096 * 64;
		static const unsigned EXPLICIT_PROT_MODE = PROT_READ | PROT_WRITE;
		static const QSocketWord MEMTRANS_CONTD = 1;
		static const QSocketWord MEMTRANS_END = 0;

		static UVAOwnership uvaown;
		static PageSet setMEPages;

    static std::vector<struct StoreLog*> *vecCriticalSectionStoreLogs;
    static std::vector<struct StoreLog*> *vecStoreLogs;
    static int sizeCriticalSectionStoreLogs = 0;
    static int sizeStoreLogs = 0;
    static bool isInCriticalSection = false;

    // BONGJUN
    static void *ptNoConstBegin;
    static void *ptNoConstEnd;

		// The range of constant pages. [ptConstBegin, ptConstEnd).
		// We assume constant pages are in both the client and the server.
		//static void *ptConstBegin;
		//static void *ptConstEnd;

		#ifdef INFLATE_READ
		static char bufInf[MEMORY_TRANSFER_UNIT * 2];
		static z_stream zInfStrm;
		#endif
		#ifdef DEFLATE_WRITE
		static char bufDef[MEMORY_TRANSFER_UNIT * 2];
		static z_stream zDefStrm;
		#endif

		#ifdef OVERHEAD_TEST
		OVERHEAD_TEST_DECL
		#endif

		static inline void* truncToPageAddr (void *addr);
		static inline void heapStateIn (QSocket *socket);
		static inline void heapStateOut (QSocket *socket);

		#ifdef DEFLATE_WRITE
		static inline size_t deflateData (void *data, size_t dsize, void *buf, size_t bsize);
		#endif

		#ifdef INFLATE_READ
		static inline size_t inflateData (void *data, size_t dsize, void *buf, size_t bsize);
		#endif

    static inline uint32_t makeInt32Addr(void *addr);
    /* not exact */
    static inline bool isUVAaddr(void *addr);
    static inline bool isUVAheapAddr(uint32_t intAddr);
    static inline bool isUVAglobalAddr(uint32_t intAddr);
		
    /*** Interfaces ***/
		//void UVAManager::initialize (UVAOwnership _uvaown) {
		void UVAManager::initialize (QSocket *socket) {
			// XMemoryManager must not have an initializer,
			// since we don't want to care about the order of 
			// multiple initializers.
			// XMemoryManager is better being prepared from the 
			// very beginning, even without initializing it.

			/* xmemInitialize (); */ 		// NOPE. We don't need this.

      /* BONGJUN : for telling "socket" to XMemoryManager */
      xmemInitialize(socket); // above from gwangmu implmentation. but I want to use
      vecCriticalSectionStoreLogs = new vector<struct StoreLog*>;
      vecStoreLogs = new vector<struct StoreLog*>;
			setMEPages.clear ();
			//uvaown = _uvaown;
      //socket = socket;

			#ifdef INFLATE_READ
			zInfStrm.zalloc = Z_NULL;
			zInfStrm.zfree = Z_NULL;
			zInfStrm.opaque = Z_NULL;
			zInfStrm.avail_in = 0;
			zInfStrm.next_in = Z_NULL;

			inflateInit (&zInfStrm);
			fprintf (stderr, "system: Inflate-read mode on\n");
			#endif

			#ifdef DEFLATE_WRITE
			zDefStrm.zalloc = Z_NULL;
			zDefStrm.zfree = Z_NULL;
			zDefStrm.opaque = Z_NULL;

			deflateInit (&zDefStrm, DEFLATE_WRITE);
			fprintf (stderr, "system: Deflate-write mode on (level: %d)\n", DEFLATE_WRITE);
			#endif

			// install a page-mapped callback method.
			// (Actually it's a 'signal handler', not a 'callback'.)
			// now PAGE_MAPPED_CALL_BACK is called whenever
			// XMemoryManager maps additional pages.
			xmemSetPageMappedCallBack (pageMappedCallBack);

			#ifdef OVERHEAD_TEST
			OHDTEST_SETUP ();
			#endif
		}


		/*** UVA Manager ***/
		/* @detail receive state synchronization command.
		 * 	State Synchronization corrects all inconsistent page states. */
		void UVAManager::synchIn (QSocket *socket) {
			/* import heap state */
			heapStateIn (socket);

			/* correct in reference to MODIFIED/EXCLUSIVE pages in the remote device */
			for (bool contd = (bool)socket->receiveWord (); contd;
					 contd = (bool)socket->receiveWord ()) {
				socket->receiveQue ();
				void *ptStart = (void *)socket->takeWord (); 			/** Start address <**/
				size_t sizePages = (size_t)socket->takeWord ();		/** Original size of pages <**/
				
				/* change state to INVALID */
				xmemPageUnmap (ptStart, sizePages);
				for (size_t off = 0; off < sizePages; off += XMEM_PAGE_SIZE) {
					setMEPages.erase ((XmemUintPtr)ptStart + off);
				}
			}

			// NOTE: Shared/Invalid page consistency checking is optimized out.
			// NOTE: Constant range integrity checking is optimized out.

			DEBUG_STMT (fprintf (stderr, "synchronized in.\n"));
		}

		/* @detail send state synchronization command. */
		void UVAManager::synchOut (QSocket *socket) {
			/* export heap state */
			heapStateOut (socket);

			/* report MODIFIED/EXCLUSIVE pages */
			XmemUintPtr uptPended = 0;
			size_t sizePended = 0;
			for (XmemUintPtr pt = setMEPages.begin (), pnext = setMEPages.next(pt);
					 pt != setMEPages.end (); 
					 pt = setMEPages.next (pt), pnext = setMEPages.next (pnext)) {
				if (!uptPended)
					uptPended = pt;
				sizePended += XMEM_PAGE_SIZE;

				// If the page is final or discontinued to the next,
				// send pended pages in the unit of MEMORY_TRANSFER_UNIT.
				if (pnext == setMEPages.end () || pnext - pt > XMEM_PAGE_SIZE) {
					long long sizeRemain = sizePended;
					for (XmemUintPtr uptSend = uptPended; sizeRemain > 0; 
							 uptSend += MEMORY_TRANSFER_UNIT, sizeRemain -= MEMORY_TRANSFER_UNIT) {
						size_t sizeSend = (MEMORY_TRANSFER_UNIT < sizeRemain) ? MEMORY_TRANSFER_UNIT : sizeRemain;

						socket->sendWord (MEMTRANS_CONTD);

						socket->pushWord (QSOCKET_WORD (uptSend));
						socket->pushWord (QSOCKET_WORD (sizeSend));
						socket->sendQue ();
					}
				
					// Reset continuity check states
					uptPended = 0;
					sizePended = 0;
				}
			}
			socket->sendWord (MEMTRANS_END);

			// NOTE: Shared/Invalid page consistency checking is optimized out.
			// NOTE: Constant range integrity checking is optimized out.

			DEBUG_STMT (fprintf (stderr, "synchronized out.\n"));
		}


		/* @detail fetch the remote page, ADDR. */
		void UVAManager::fetchIn (QSocket *socket, void *addr) {
			void *paddr = truncToPageAddr (addr);

			//assert ((paddr < ptConstBegin || paddr >= ptConstEnd) && "constant pages cannot be fetched");

			assert (addr != NULL && "null demanded page");
			DEBUG_STMT (fprintf (stderr, "demanded_page:%p\n", (void *)paddr));

			void *res = xmemPagemap (paddr, XMEM_PAGE_SIZE, false);
			assert (res != (void *)0xFFFFFFFF && 
				"UVAManager::fetchIn() page mapping failed");

			/* Receive page	*/
			socket->receiveRange (paddr, XMEM_PAGE_SIZE);

			/* adjust to SHARED state */
			// NOTE: SHARED set is optimized out.

			/* set protection to detect a new MODIFIED/EXCLUSIVE page */
			xmemSetProtMode (paddr, XMEM_PAGE_SIZE, PROT_READ);				
		}

		/* @detail receive a fetch request from the remote device,
		 * 	responding with the requested page. 
		 * 	After responding, adjust the page's state to SHARED. */
		void UVAManager::fetchOut (QSocket *socket, void *addr) {
			void *paddr = truncToPageAddr (addr);

			//assert ((paddr < ptConstBegin || paddr >= ptConstEnd) && 
			//		"constant pages cannot be fetched");

			DEBUG_STMT (fprintf (stderr, "page_requested:%p\n", paddr));
			socket->sendRange (paddr, XMEM_PAGE_SIZE);
			DEBUG_STMT (fprintf (stderr, "page sent\n"));

			/* adjust MODIFIED/EXCLUSIVE state to SHARED state */
			setMEPages.erase ((XmemUintPtr)paddr);

			// NOTE: SHARED set is optimized out.
		}


		/* @detail receive all 'flushed' pages.
		 * 	Memory Flushing results in the receiver's memory to be complete,
		 * 	where the receiver has no longer INVALID pages. 
		 * 	NOTE: This is an ideal. If the sender missed some EXCLUSIVE/MODIFIED
		 * 	pages whose state is presumably INVALID for the receiver,
		 * 	the receiver has no chance but to fetch them on the fly. */
		void UVAManager::flushIn (QSocket *socket) {
			/* import heap state */
			heapStateIn (socket);

			// Receive memory until termination signal
			for (bool contd = (bool)socket->receiveWord (); contd;
					 contd = (bool)socket->receiveWord ()) {
				socket->receiveQue ();
				void *ptStart = (void *)socket->takeWord (); 			/** Start address <**/
				size_t sizePages = (size_t)socket->takeWord ();		/** Original size of pages <**/
				#ifdef INFLATE_READ
				size_t sizeChunk = (size_t)socket->takeWord ();		/** Transfered chunk size <**/
				#endif

				DEBUG_STMT (fprintf (stderr, "ptStart:%p, sizePages:%d\n", ptStart, sizePages));
				assert (((XmemUintPtr)ptStart & XMEM_PAGE_MASK_INV) == 0 && 
					"UVAManager::flushIn() received un-aligned address");
				assert ((sizePages & XMEM_PAGE_MASK_INV) == 0 && 
					"UVAManager::flushIn() received un-aligned pages size");

				/* change state to SHARED */
				xmemPagemap (ptStart, sizePages, false);
				for (size_t off = 0; off < sizePages; off += XMEM_PAGE_SIZE) {
					setMEPages.erase ((XmemUintPtr)ptStart + off);
				}

				#ifdef INFLATE_READ
				socket->receiveRange (bufInf, sizeChunk);
				inflateData (bufInf, sizeChunk, ptStart, sizePages);
				#else
				socket->receiveRange (ptStart, sizePages);
				#endif
				DEBUG_STMT (fprintf (stderr, "page installed\n"));

				/* enforce 'read' protection to monitor EXCLUSIVE pages */
				xmemSetProtMode (ptStart, sizePages, PROT_READ);
			}

			DEBUG_STMT (fprintf (stderr, "sizeHeap: 0x%x\n", xmemGetHeapSize ()));
			
			// NOTE: Constant range integrity checking is optimized out.
		}

		/* @detail send a 'flush' command, and flush out the memory. */
		void UVAManager::flushOut (QSocket *socket) {
			// Renew heap state
			heapStateOut (socket);

			DEBUG_STMT (fprintf (stderr, "setMEPages.begin (): 0x%x\n", setMEPages.begin ()));
			DEBUG_STMT (fprintf (stderr, "pageBegin: 0x%x, pageEnd: 0x%x\n", xmemPageBegin (), xmemPageEnd ()));
			DEBUG_STMT (fprintf (stderr, "sizeHeap: 0x%x\n", xmemGetHeapSize ()));

			#ifdef PRINT_MEMORY_TRANSFER_INFO
			unsigned sizeMem = 0;
			#	ifdef DEFLATE_WRITE
			unsigned sizeComped = 0;
			# endif
			#endif

			// Find continuous page chunk
			XmemUintPtr uptPended = 0;
			size_t sizePended = 0;
			for (XmemUintPtr pt = setMEPages.begin (), pnext = setMEPages.next(pt);
					 pt != setMEPages.end (); 
					 pt = setMEPages.next (pt), pnext = setMEPages.next (pnext)) {
				if (!uptPended)
					uptPended = pt;
				sizePended += XMEM_PAGE_SIZE;

				// If the page is final or discontinued to the next,
				// send pended pages in the unit of MEMORY_TRANSFER_UNIT.
				if (pnext == setMEPages.end () || pnext - pt > XMEM_PAGE_SIZE) {
					long long sizeRemain = sizePended;
					for (XmemUintPtr uptSend = uptPended; sizeRemain > 0; 
							 uptSend += MEMORY_TRANSFER_UNIT, sizeRemain -= MEMORY_TRANSFER_UNIT) {
						size_t sizeSend = (MEMORY_TRANSFER_UNIT < sizeRemain) ? MEMORY_TRANSFER_UNIT : sizeRemain;

						socket->sendWord (MEMTRANS_CONTD);

						socket->pushWord (QSOCKET_WORD (uptSend));
						socket->pushWord (QSOCKET_WORD (sizeSend));
						DEBUG_STMT (fprintf (stderr, "addr:%p, sizeSend:%u\n", (void *)uptSend, sizeSend));

						#ifdef DEFLATE_WRITE
						size_t sizeChunk = deflateData ((void *)uptSend, sizeSend, bufDef, sizeof(bufDef));
						socket->pushWord (QSOCKET_WORD (sizeChunk));
						socket->sendQue ();
						socket->sendRange (bufDef, sizeChunk);

						#	ifdef PRINT_MEMORY_TRANSFER_INFO
						sizeComped += sizeChunk;
						#	endif
						#else
						socket->sendQue ();
						socket->sendRange ((void *)uptSend, sizeSend);
						#endif

						#ifdef PRINT_MEMORY_TRANSFER_INFO
						sizeMem += sizeSend;
						#endif
					}
				
					// Reset continuity check state
					uptPended = 0;
					sizePended = 0;
				}
			}
			socket->sendWord (MEMTRANS_END);

			/* adjust MODIFIED/EXCLUSIVE state to SHARED */
			setMEPages.clear ();

			#ifdef PRINT_MEMORY_TRANSFER_INFO
			fprintf (stderr, ">> Transferred Memory Size: %f MB\n", sizeMem / 1024.0f / 1024.0f);
			#	ifdef DEFLATE_WRITE
			fprintf (stderr, ">> Compressed Memory Size: %f MB\n", sizeComped / 1024.0f / 1024.0f);
			fprintf (stderr, ">> Compresion Ratio: %f %%\n", (1 - sizeComped / (float)sizeMem) * 100);
			#	endif
			#endif
	
			// NOTE: Constant range integrity checking is optimized out.
		}
		
		/* @detail adjust the page's state to MODIFIED/EXCLUSIVE. */
		void UVAManager::resolveModified (void *addr) {
			void *paddr = truncToPageAddr (addr);
			assert (paddr != NULL && "null modified page");

			DEBUG_STMT (fprintf (stderr, "dirty_addr:%p\n", paddr));

			/* adjust to MODIFIED state */
			setMEPages.insert ((XmemUintPtr)paddr);
			xmemSetProtMode (paddr, XMEM_PAGE_SIZE, PROT_READ | PROT_WRITE);
		}

    /* @detail HLRC (Home-based Lazy Release Consistency): acquire */
    void UVAManager::acquireHandler(QSocket *socket) {

      // send invalidate address request.
     
      socket->pushWordF(INVALID_REQ);
      socket->sendQue();
#ifdef DEBUG_UVA
          LOG("[client] send invalid request (%d)\n", INVALID_REQ);
#endif
      
      // recv invalidate address list.
      socket->receiveQue();
#ifdef DEBUG_UVA
      LOG("[client] recv address list\n");
#endif
      int mode = socket->takeWordF();
      LOG("[client] recv mode (%d)\n", mode); 
      assert(mode == INVALID_REQ_ACK && "wrong");
      

      //int addressSize = socket->takeWordF();
      int addressNum = socket->takeWordF();
      vector<void*> addressVector;
      //void** addressbuf = (void **) malloc(addressSize);
      void** addressbuf = (void **) malloc(4);
      for(int i = 0; i < addressNum; i++) {
        //socket->takeRangeF(addressbuf, addressSize);
        socket->takeRangeF(addressbuf, 4);
        addressVector.push_back(*addressbuf);
      }
      free(addressbuf);
      // all address invalidate.
      
      for(vector<void*>::iterator it = addressVector.begin(); it != addressVector.end(); it++) {
        void* address = *it;
#ifdef DEBUG_UVA
        LOG("invalidate address : %p\n", address);
#endif
        mprotect(truncToPageAddr(address), PAGE_SIZE, PROT_NONE);
      }

      isInCriticalSection = true;
#ifdef DEBUG_UVA
          LOG("[client] acquire handler end (%d)\n", addressNum);
#endif
    }

    /* @detail HLRC (Home-based Lazy Release Consistency): release */
    void UVAManager::releaseHandler(QSocket *socket) {
      /* At first, make store logs to be send to Home */
      void *storeLogs = malloc(sizeCriticalSectionStoreLogs);
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
      int i = 0;
      while (current != intAddrOfStoreLogs + sizeStoreLogs) {
        struct StoreLog* curStoreLog = (*vecCriticalSectionStoreLogs)[i];
#ifdef DEBUG_UVA
        LOG("[client] CRITICAL SECTION in while | curStoreLog (size:%d, data:%p, data:%d, addr:%p)\n", curStoreLog->size, curStoreLog->data, *((int*)curStoreLog->data), curStoreLog->addr);
#endif        
        uint32_t intAddr;
        memcpy(reinterpret_cast<void*>(current), &curStoreLog->size, 4);
        memcpy(reinterpret_cast<void*>(current+4), curStoreLog->data, curStoreLog->size);
        memcpy(&intAddr, &curStoreLog->addr, 4);
        memcpy(reinterpret_cast<void*>(current+4+curStoreLog->size), &intAddr, 4);
        current = current + 8 + curStoreLog->size;
        //free(curStoreLog->data);
        i++;
      }
#ifdef DEBUG_UVA
      LOG("[client] CRITICAL SECTION # of vecStoreLogs %d | sizeStoreLogs %d\n", vecCriticalSectionStoreLogs->size(), sizeCriticalSectionStoreLogs);
#endif 
      /* Second, send them all */
      socket->pushWordF(RELEASE_REQ);
      socket->pushWordF(sizeCriticalSectionStoreLogs);
      socket->pushRange(storeLogs, sizeCriticalSectionStoreLogs);
      socket->sendQue();
      sizeCriticalSectionStoreLogs = 0;
      free(storeLogs);
      vecCriticalSectionStoreLogs->clear();
      isInCriticalSection = false;
    }

    /* XXX question XXX
     * 1. all stores should be stored in slog for sync?
     * 2. release -> acquire in sync? 
     */
    /* @detail Sync operation (mixing acquire & release) */
    void UVAManager::syncHandler(QSocket *socket) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      /* At first, make store logs to be send to Home */
      void *storeLogs = malloc(sizeStoreLogs);
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
      int i = 0;
      while (current != intAddrOfStoreLogs + sizeStoreLogs) {
        struct StoreLog* curStoreLog = (*vecStoreLogs)[i];
#ifdef DEBUG_UVA
        LOG("[client] in while | curStoreLog (size:%d, data:%p, data:%d, addr:%p)\n", curStoreLog->size, curStoreLog->data, *((int*)curStoreLog->data), curStoreLog->addr);
#endif        
        uint32_t intAddr;
        memcpy(reinterpret_cast<void*>(current), &curStoreLog->size, 4);
        memcpy(reinterpret_cast<void*>(current+4), curStoreLog->data, curStoreLog->size);
        memcpy(&intAddr, &curStoreLog->addr, 4);
        memcpy(reinterpret_cast<void*>(current+4+curStoreLog->size), &intAddr, 4);
        current = current + 8 + curStoreLog->size;
        //free(curStoreLog->data);
        i++;
      }
#ifdef DEBUG_UVA
      LOG("[client] # of vecStoreLogs %d | sizeStoreLogs %d\n", vecStoreLogs->size(), sizeStoreLogs);
#endif 
      /* Second, send them all */
      socket->pushWordF(SYNC_REQ);
      socket->pushWordF(sizeStoreLogs);
      if (sizeStoreLogs != 0)
        socket->pushRange(storeLogs, sizeStoreLogs);
      socket->sendQue();
      sizeStoreLogs = 0;
      free(storeLogs);
      vecStoreLogs->clear(); // XXX

      /* Third, recv invalidate address list. */
      socket->receiveQue();
#ifdef DEBUG_UVA
      LOG("[client] recv address list\n");
#endif
      //int addressSize = socket->takeWordF(); // XXX: Currently, out UVA address space is 32 bits.
      int addressNum = socket->takeWordF();
      vector<void*> addressVector;
      //void** addressbuf = (void **) malloc(addressSize);
      void** addressbuf = (void **) malloc(4);
      for(int i = 0; i < addressNum; i++) {
        //socket->takeRangeF(addressbuf, addressSize);
        socket->takeRangeF(addressbuf, 4);
        addressVector.push_back(*addressbuf);
      }
      free(addressbuf);
      // all address invalidate.
      
      for(vector<void*>::iterator it = addressVector.begin(); it != addressVector.end(); it++) {
        void* address = *it;
#ifdef DEBUG_UVA
        //LOG("invalidate address : %p\n", address);
#endif
        mprotect(truncToPageAddr(address), PAGE_SIZE, PROT_NONE);
      }

      //isInCriticalSection = true;
#ifdef DEBUG_UVA
      LOG("[client] sync handler end (%d)\n", addressNum);
#endif
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "SYNC %lf %d\n", watch.diff(), 8 + sizeStoreLogs + 4 + (4 *addressNum));
      fclose(fp);
#endif
    }

    /*** Load/Store Handler @@@@@@@@ BONGJUN @@@@@@@@ ***/
    void UVAManager::loadHandler(QSocket *socket, size_t typeLen, void *addr) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      uint32_t intAddr = makeInt32Addr(addr);
      if(!(isUVAheapAddr(intAddr) || isUVAglobalAddr(intAddr))) return;

      if(xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) {
#ifdef DEBUG_UVA
        LOG("[client] Load : loadHandler start (%p)\n", addr);
        if (xmemIsHeapAddr(addr)) {
          LOG("[client] Load : isHeapAddr, going to request | addr %p, typeLen %lu\n", addr, typeLen);
        } else if (isFixedGlobalAddr(addr)) {
          LOG("[client] Load : isFixedGlobalAddr, going to request | addr %p, typeLen %lu\n", addr, typeLen);
        }
#endif
        socket->pushWordF(LOAD_REQ); // mode 2 (client -> server : load request)
        //socket->pushWordF(sizeof(addr));
        socket->pushWordF(typeLen); // type length
        //LOG("[client] DEBUG : may be before segfault?\n");
        

#ifdef DEBUG_UVA
        LOG("[client] intAddr %d\n", intAddr);
#endif
        socket->pushWordF(intAddr);
        socket->sendQue();

        socket->receiveQue();
        int mode = socket->takeWord();
#ifdef DEBUG_UVA
        LOG("[client] mode : %d\n", mode); // should be 3
#endif
        //assert(mode == LOAD_REQ_ACK && "wrong");
        int len = socket->takeWord();
        //LOG("[client] len : %d\n", len);
        void *buf = malloc(len);

        socket->takeRangeF(buf, len);
        memcpy(addr, buf, len);
        free(buf);
#ifdef DEBUG_UVA
        hexdump("load", addr, typeLen);
        //xmemDumpRange(addr, typeLen);
        LOG("[client] Load : loadHandler END\n\n");
#endif
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "LOAD %lf\n", watch.diff());
      fclose(fp);
#endif
    }

    void UVAManager::storeHandler(QSocket *socket, size_t typeLen, void *data, void *addr) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      uint32_t intAddr = makeInt32Addr(addr);
      if(!(isUVAheapAddr(intAddr) || isUVAglobalAddr(intAddr))) return;

      if (xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) { 
#ifdef DEBUG_UVA
        LOG("[client] Store : storeHandler start (%p)\n", addr);
#endif
        if (xmemIsHeapAddr(addr)) {
#ifdef DEBUG_UVA
          LOG("[client] Store : isHeapAddr, is going to request | addr %p, typeLen %lu\n", addr, typeLen);
#endif
        } else if (isFixedGlobalAddr(addr)) {
#ifdef DEBUG_UVA
          LOG("[client] Store : isFixedGlobalAddr, is going to request | addr %p, typeLen %lu\n", addr, typeLen);
#endif
        }
        socket->pushWordF(STORE_REQ);
        socket->pushWordF(typeLen);

        socket->pushWordF(intAddr);
        socket->pushRangeF(&data, typeLen);
        socket->sendQue();
#ifdef DEBUG_UVA
        LOG("[client] Store : sizeof(addr) %d, data length %d\n", sizeof(addr), typeLen);
#endif

        socket->receiveQue();
        int mode = socket->takeWord();
#ifdef DEBUG_UVA
        LOG("[client] mode : %d\n", mode);
#endif
        //assert(mode == STORE_REQ_ACK && "wrong");
        int len = socket->takeWord();
        if (len == 0) { // Normal ack
          //LOG("[client] TEST stored value : %d\n", *((int*)addr));
        } else if (len == -1) {
#ifdef DEBUG_UVA
          LOG("[client] store request fail\n"); // TODO: have to handler failure situation.
#endif
        } else {
          assert(0 && "error: undefined behavior");
        }
#ifdef DEBUG_UVA
        LOG("[client] Store : storeHandler END\n\n");
#endif
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "STORE %lf\n", watch.diff());
      fclose(fp);
#endif
    }

    void *UVAManager::memsetHandler(QSocket *socket, void *addr, int value, size_t num) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      uint32_t intAddr = makeInt32Addr(addr);
      if(!(isUVAheapAddr(intAddr) || isUVAglobalAddr(intAddr))) return addr;

      if (xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) {
#ifdef DEBUG_UVA
        LOG("[client] Memset : memsetHandler start (%p)\n", addr);
        if (xmemIsHeapAddr(addr)) {
          LOG("[client] Memset : isHeapAddr, is going to request | addr %p\n", addr);
        } else if (isFixedGlobalAddr(addr)) {
          LOG("[client] Memset : isFixedGlobalAddr, is going to request | addr %p\n", addr);
        }
#endif
        
        socket->pushWordF(MEMSET_REQ);
        socket->pushWordF(intAddr);
        socket->pushWordF(value);
        socket->pushWordF(num); // XXX check
        socket->sendQue();
#ifdef DEBUG_UVA
        LOG("[client] Memset : memset(%p, %d, %d)\n", addr, value, num);
#endif

        socket->receiveQue();
        int ack = socket->takeWordF();
#ifdef DEBUG_UVA
        printf("MEMSET: ack %d\n", ack);
#endif
        //assert(ack == MEMSET_REQ_ACK && "wrong");
        //assert(ack == 9 && "wrong");

#ifdef DEBUG_UVA
        LOG("[client] Memset : memsetHandler END (%p)\n\n", addr);
#endif
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "MEMSET %lf\n", watch.diff());
      fclose(fp);
#endif
      return addr;
    }

    void *UVAManager::memcpyHandler(QSocket *socket, void *dest, void *src, size_t num) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      if((long)dest > 0xffffffff && (long)src > 0xffffffff)
        return dest;
      uint32_t intDest = makeInt32Addr(dest);
      uint32_t intSrc = makeInt32Addr(src);
      /** typeMemcpy
       * 0: not related with UVA
       * 1: dest is in UVA addr space (src is in wherever)
       * 2: src is in UVA addr space (dest is not in UVA addr space)
       **/
      int typeMemcpy = 0;
#ifdef DEBUG_UVA
      LOG("[client] Memcpy : destination = %u, src = %u\n",intDest, intSrc);
#endif
      if(isUVAheapAddr(intDest) || isUVAglobalAddr(intDest)) {
        typeMemcpy = 1;
      } else if (isUVAheapAddr(intSrc) || isUVAglobalAddr(intSrc)) {
        typeMemcpy = 2;
      } else {
        return dest;
      }

      if (typeMemcpy) {
#ifdef DEBUG_UVA
        LOG("[client] Memcpy : memcpyHandler start (%p <- %p) | typeMemcpy (%d)\n", dest, src, typeMemcpy);
        if (xmemIsHeapAddr(dest)) {
          LOG("[client] Memcpy : dest isHeapAddr, is going to request | %p <- %p\n", dest, src);
        } else if (isFixedGlobalAddr(dest)) {
          LOG("[client] Memcpy : dest isFixedGlobalAddr, is going to request | %p <- %p\n", dest, src);
        } else if (xmemIsHeapAddr(src)) {
          LOG("[client] Memcpy : src isHeapAddr, is going to request | %p <- %p\n", dest, src);
        } else if (isFixedGlobalAddr(src)) {
          LOG("[client] Memcpy : src isFixedGlobalAddr, is going to request | %p <- %p\n", dest, src);
        }
#endif
         
        socket->pushWordF(MEMCPY_REQ);
        socket->pushWordF(typeMemcpy);
        if (typeMemcpy == 1) {
          socket->pushWordF(intDest);
          socket->pushWordF(num);
          socket->pushRangeF(src, num);
        
          socket->sendQue();
          socket->receiveQue();
          int ack = socket->takeWord();
          assert(ack == MEMCPY_REQ_ACK && "wrong");
        } else if (typeMemcpy == 2) {
          /*  if typeMemcpy is 2, we have to load "src".  
           *  dest is out of scope in this case.
           */
#ifdef DEBUG_UVA
          LOG("[client] Memcpy : intSrc %d\n", intSrc);
#endif
          socket->pushWordF(intSrc);
          socket->pushWordF(num);
          socket->sendQue();

          socket->receiveQue();
          socket->takeRangeF(src, num);
        } else {
          assert(0);
        }
#ifdef DEBUG_UVA
        LOG("[client] Memcpy : memcpyHandler END (%p <- %p, %d)\n\n", dest, src, num);
#endif
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "MEMCPY %lf\n", watch.diff());
      fclose(fp);
#endif
      return dest;
    }

		/*** Get/Set Interfaces ***/
		void UVAManager::setConstantRange (void *begin_noconst, void *end_noconst/*, void *begin_const, void *end_const*/) { /* FIXME */
      ptNoConstBegin = truncToPageAddr (begin_noconst);
      ptNoConstEnd = truncToPageAddr ((void *)((XmemUintPtr)end_noconst + XMEM_PAGE_SIZE - 1));
			//ptConstBegin = truncToPageAddr (begin_const);
			//ptConstEnd = truncToPageAddr ((void *)((XmemUintPtr)end_const + XMEM_PAGE_SIZE - 1));

#ifdef DEBUG_UVA
      LOG("ptNoConst (%p~%p) \n", ptNoConstBegin, ptNoConstEnd/*, ptConstBegin, ptConstEnd*/);
#endif
			// FIXME: To enforce the constantness of the given range,
			// 	We should rule out pages in the range from the EXCLUSIVE set
			// 	whenever consistency operations (such as 'synch', 'flush') are done.
			// 	But for the sake of the performance, it would be better to
			// 	rule out the pages just once (i.e. when this function is called),
			// 	and believe that the constantness will not be harmed.
			/*bool turnaround_guard = false;
			for (XmemUintPtr upt = (XmemUintPtr)ptConstBegin; upt < (XmemUintPtr)ptConstEnd; upt += XMEM_PAGE_SIZE) {
				if (upt == NULL && turnaround_guard) break;
				turnaround_guard = true;
			
				setMEPages.erase (upt);
			}*/
		}

    void UVAManager::storeHandlerForHLRC(QSocket *socket, size_t typeLen, void *data, void *addr) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
#ifdef DEBUG_UVA
      LOG("[client] storeHandlerForHLRC START (isInCriticalSection %d)\n", isInCriticalSection);
#endif
      uint32_t intAddr = makeInt32Addr(addr);
      if(!isUVAaddr(addr)) {
#ifdef UVA_EVAL
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "STORE %lf %d\n", watch.diff(), typeLen);
        fclose(fp);
#endif
        return;
      }
#ifdef DEBUG_UVA
      LOG("[client] in storeLog (size:%d, addr:%p, data:%p)\n", typeLen, addr, data);
#endif

      void *tmpData = malloc(typeLen);
      memcpy(tmpData, &data, typeLen);
#ifdef DEBUG_UVA
      //LOG("[client] tmpData (data:%d)\n", *((int*)tmpData));
#endif
      struct StoreLog* slog = new StoreLog (static_cast<int>(typeLen), tmpData, addr);
      if(!isInCriticalSection) {
        vecStoreLogs->push_back(slog);
        sizeStoreLogs = sizeStoreLogs + 8 + typeLen;
      } else {
        vecCriticalSectionStoreLogs->push_back(slog);
        sizeCriticalSectionStoreLogs = sizeCriticalSectionStoreLogs + 8 + typeLen;
      }
#ifdef DEBUG_UVA
      LOG("[client] storeHandlerForHLRC END\n\n");
#endif
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "STORE %lf\n", watch.diff());
      fclose(fp);
#endif
    }

    void *UVAManager::memsetHandlerForHLRC(QSocket *socket, void *addr, int value, size_t num) {
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      uint32_t intAddr = makeInt32Addr(addr);
      if(!isUVAaddr(addr)) {
#ifdef UVA_EVAL
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "MEMSET %lf %d\n", watch.diff(), num);
        fclose(fp);
#endif
        return addr;
      }
      
      void *tmpValue = malloc(num);
      memcpy(tmpValue, &value, num);
      struct StoreLog* slog = new StoreLog (static_cast<int>(num), tmpValue, addr);
      if(!isInCriticalSection) {
        vecStoreLogs->push_back(slog);
        sizeStoreLogs = sizeStoreLogs + 8 + num;
      } else {
        vecCriticalSectionStoreLogs->push_back(slog);
        sizeCriticalSectionStoreLogs = sizeCriticalSectionStoreLogs + 8 + num;
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "MEMSET %lf\n", watch.diff());
      fclose(fp);
#endif
      return addr;
    }

    void *UVAManager::memcpyHandlerForHLRC(QSocket *socket, void *dest, void *src, size_t num) {
      // XXX: no need...
      if((long)dest > 0xffffffff && (long)src > 0xffffffff)
        return dest;
#ifdef UVA_EVAL
      StopWatch watch;
      watch.start();
#endif
      uint32_t intDest = makeInt32Addr(dest);
      uint32_t intSrc = makeInt32Addr(src);
      /** typeMemcpy
       * 0: not related with UVA
       * 1: dest is in UVA addr space (src is in wherever)
       * 2: src is in UVA addr space (dest is not in UVA addr space)
       **/
      int typeMemcpy = 0;
#ifdef DEBUG_UVA
      LOG("[client] HLRC Memcpy : destination = %u(%p), src = %u(%p)\n",intDest, dest, intSrc, src);
#endif
      if(isUVAaddr(dest)) {
        typeMemcpy = 1; // dest is in UVA
      } else if (isUVAaddr(src)) {
        typeMemcpy = 2; // src is in UVA
        //return dest; // CHECK
      } else {
#ifdef UVA_EVAL
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "MEMCPY %lf\n", watch.diff());
        fclose(fp);
#endif
        return dest;
      }

      /* XXX CONCERN: what if typeMemcpy is 2? XXX 
       * 
       * If the page which "src" is included was invalidated in acquire,
       * segmentation fault will be occurred at real memcpy call after this
       * memcpyHandlerForHLRC function and be handled properly.  
       * 
       * If not, memcpy will be executed normally. In both case, "Home" doesn't
       * need to know that this memcpy into src is occurred.
       * 
       */
      if (typeMemcpy == 1) { // dest is in UVA
#ifdef DEBUG_UVA
        LOG("[client] HLRC Memcpy : typeMemcpy (1), slog { %d, %p, %p }\n", num, src, dest);
#endif
        void *tmpData = malloc(num);
        memcpy(tmpData, src, num);
#ifdef DEBUG_UVA
        LOG("[client] HLRC Memcpy : tmpData (%d)\n", *((int*)tmpData));
#endif
        struct StoreLog *slog = new StoreLog ( static_cast<int>(num), tmpData, dest );
        if(isInCriticalSection) {
          vecCriticalSectionStoreLogs->push_back(slog);
          sizeCriticalSectionStoreLogs = sizeStoreLogs + 8 + num;
        } else {
          vecStoreLogs->push_back(slog);
          sizeStoreLogs = sizeStoreLogs + 8 + num;
        }
      } else if (typeMemcpy == 2) {
        /*  if typeMemcpy is 2, we have to load "src".  
         *  dest is out of scope in this case.
         */
#ifdef DEBUG_UVA
        LOG("[client] HLRC Memcpy : typeMemcpy (2), intSrc %p(%u)\n", src, intSrc);
#endif
        // XXX Is it OK?
        if(mmap(truncToPageAddr(src), num + PAGE_SIZE - num % 4096, EXPLICIT_PROT_MODE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, (off_t)0) == MAP_FAILED){
          perror("mmap");
          assert(0 && "[client] mmap failed");     
        }
        socket->pushWordF(MEMCPY_REQ);
        socket->pushWordF(2); // typeMemcpy == 2
        socket->pushWordF(intSrc);
        socket->pushWordF(num);
        socket->sendQue();

        socket->receiveQue();
        socket->takeRangeF(src, num);
#ifdef DEBUG_UVA
        hexdump("memcpy", src, 30);
#endif
      }
#ifdef UVA_EVAL
      watch.end();
      FILE *fp = fopen("uva-eval.txt", "a");
      fprintf(fp, "MEMCPY %lf %d\n", watch.diff(), 16 + num);
      fclose(fp);
#endif
      return dest;
    }

		size_t UVAManager::getHeapSize () {
			return xmemGetHeapSize ();
		}

		bool UVAManager::hasPage (void *addr) {
			void *paddr = truncToPageAddr (addr);
			return xmemIsMapped (paddr);
		}
		
    // XXX: by BONGJUN for fixed global
    bool UVAManager::isFixedGlobalAddr (void *addr) {
      if ((void*)0x15000000 <= addr && addr < (void*)0x16000000) /* FIXME: upper bound should be ptConstEnd. and below elseif should be erased. */{
        //printf("UVAManager::isFixedGlobalAddr: (%p) ~ (%p) / addr (%p)\n", (void*)0x15000000, ptConstEnd, addr); 
        return true;
      /*} else if (ptConstBegin <= addr && addr < ptConstEnd) {
        printf("UVAManager::isFixedGlobalAddr: (%p) ~ (%p) / addr (%p)\n", ptConstBegin, ptConstEnd, addr);
        return true;*/
      } else 
        return false;
    }

    void UVAManager::getFixedGlobalAddrRange (void **begin_noconst, void **end_noconst/*, void **begin_const, void **end_const*/) {
      *begin_noconst = ptNoConstBegin;
      *end_noconst = ptNoConstEnd;
      //*begin_const = ptConstBegin;
      //*end_const = ptConstEnd;
    }

    static inline uint32_t makeInt32Addr(void *addr) {
      uint32_t intAddr;
      memcpy(&intAddr, &addr, 4);
      return intAddr;
    }

    /* not exact */
    static inline bool isUVAaddr(void *addr) {
      if ((long)addr > 0xffffffff)
        return false;
      uint32_t intAddr = makeInt32Addr(addr);
      return (isUVAheapAddr(intAddr) || isUVAglobalAddr(intAddr));
    }

    static inline bool isUVAheapAddr(uint32_t intAddr) {
      if (402653184 <= intAddr && intAddr < 939524096)
        return true;
      else
        return false;
    }
    
    static inline bool isUVAglobalAddr(uint32_t intAddr) {
      if (352321536 <= intAddr && intAddr < 369098752)
        return true;
      else
        return false;
    }

		/*** CallBack ***/
		void UVAManager::pageMappedCallBack (XmemUintPtr paddr) {
			/* set to MODIFIED/EXCLUSIVE state */
#ifdef DEBUG_UVA
      printf("UVAManger::pageMappedCallBack: paddr (%p)\n", paddr);
#endif
			setMEPages.insert (paddr);
		}


		/*** Internals ***/
		static inline void* truncToPageAddr (void *addr) {
			return (void *)((XmemUintPtr)addr & XMEM_PAGE_MASK);
		}

		static inline void heapStateIn (QSocket *socket) {
			XmemStateInfo xsinfo;
			socket->receiveRange (&xsinfo, XSINFO_SIZE);
			xmemImportState (xsinfo);
		}

		static inline void heapStateOut (QSocket *socket) {
			XmemStateInfo xsinfo;
			xmemExportState (&xsinfo);
			socket->sendRange (&xsinfo, XSINFO_SIZE);
		}

		#ifdef DEFLATE_WRITE
		static inline size_t deflateData (void *data, size_t dsize, void *buf, size_t bsize) {
			zDefStrm.next_in = reinterpret_cast<uint8_t *>(data);
			zDefStrm.avail_in = dsize;
			
			zDefStrm.next_out = reinterpret_cast<uint8_t *>(buf);
			zDefStrm.avail_out = bsize;

			int ret = deflate (&zDefStrm, Z_SYNC_FLUSH);
			if (ret != Z_OK)
				return 0;

			return bsize - zDefStrm.avail_out;
		}
		#endif

		#ifdef INFLATE_READ
		static inline size_t inflateData (void *data, size_t dsize, void *buf, size_t bsize) {
			zInfStrm.next_in = reinterpret_cast<uint8_t *>(data);
			zInfStrm.avail_in = dsize;

			zInfStrm.next_out = reinterpret_cast<uint8_t *>(buf);
			zInfStrm.avail_out = bsize;
			
			int ret = inflate (&zInfStrm, Z_SYNC_FLUSH);
			if (ret == Z_STREAM_ERROR) {
				DEBUG_STMT (fprintf (stderr, "data: %p, dsize:%d, buf:%p, bsize: %d\n", data, dsize, buf, bsize));
				assert (ret != Z_STREAM_ERROR);
			}
			
			return bsize - zInfStrm.avail_out;
		}
		#endif
	}
}

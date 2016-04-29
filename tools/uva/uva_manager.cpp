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
#include "uva_manager.h"
#include "packet_header.h"
#include "pageset.h"
#include "xmem_alloc.h"
#include "xmem_info.h"
#include "mm.h" // XXX hmm.... BONGJUN
#include "debug.h"
#include "log.h"
#include "hexdump.h"

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

//using namespace std;

namespace corelab {
	namespace UVA {
		static const unsigned MEMORY_TRANSFER_UNIT = 4096 * 64;
		static const QSocketWord MEMTRANS_CONTD = 1;
		static const QSocketWord MEMTRANS_END = 0;

		static UVAOwnership uvaown;
		static PageSet setMEPages;

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

    enum {
      THREAD_EXIT = -1,
      HEAP_ALLOC_REQ = 0,
      HEAP_ALLOC_REQ_ACK = 1,
      LOAD_REQ = 2,
      LOAD_REQ_ACK = 3,
      STORE_REQ = 4,
      STORE_REQ_ACK = 5,
      MMAP_REQ = 6,
      MMAP_REQ_ACK = 7,
      MEMSET_REQ = 8,
      MEMSET_REQ_ACK = 9,
      MEMCPY_REQ = 10,
      MEMCPY_REQ_ACK = 11,
      MEMMOVE_REQ = 12,
      MEMMOVE_REQ_ACK = 13,
      GLOBAL_SEGFAULT_REQ = 30, 
      GLOBAL_SEGFAULT_REQ_ACK = 31, 
      GLOBAL_INIT_COMPLETE_SIG = 32,
      GLOBAL_INIT_COMPLETE_SIG_ACK = 33
    };

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


    /*** Load/Store Handler @@@@@@@@ BONGJUN @@@@@@@@ ***/
    void UVAManager::loadHandler(QSocket *socket, size_t typeLen, void *addr) {
      if(xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) {
        LOG("[client] Load : loadHandler start (%p)\n", addr);
        if (xmemIsHeapAddr(addr)) {
          LOG("[client] Load : isHeapAddr, going to request | addr %p, typeLen %lu\n", addr, typeLen);
        } else if (isFixedGlobalAddr(addr)) {
          LOG("[client] Load : isFixedGlobalAddr, going to request | addr %p, typeLen %lu\n", addr, typeLen);
        }
        socket->pushWordF(LOAD_REQ); // mode 2 (client -> server : load request)
        //socket->pushWordF(sizeof(addr));
        socket->pushWordF(typeLen); // type length
        //LOG("[client] DEBUG : may be before segfault?\n");
        
        uint32_t intAddr;
       
        memcpy(&intAddr, &addr, 4);

        LOG("[client] intAddr %d\n", intAddr);
        socket->pushWordF(intAddr);
        socket->sendQue();

        socket->receiveQue();
        int mode = socket->takeWord();
        //LOG("[client] mode : %d\n", mode); // should be 3
        assert(mode == LOAD_REQ_ACK && "wrong");
        int len = socket->takeWord();
        //LOG("[client] len : %d\n", len);
        void *buf = malloc(len);

        socket->takeRangeF(buf, len);
        memcpy(addr, buf, len);
        hexdump("load", addr, typeLen);
        //xmemDumpRange(addr, typeLen);
        LOG("[client] Load : loadHandler END\n\n");
      }
    }

    void UVAManager::storeHandler(QSocket *socket, size_t typeLen, void *data, void *addr) {
      //LOG("[client] Store instr, addr %p, typeLen %lu, TEST val %d\n", addr, typeLen, *((int*)addr)); 
      if (xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) { 
        LOG("[client] Store : storeHandler start (%p)\n", addr);
        if (xmemIsHeapAddr(addr)) {
          LOG("[client] Store : isHeapAddr, is going to request | addr %p, typeLen %lu\n", addr, typeLen);
        } else if (isFixedGlobalAddr(addr)) {
          LOG("[client] Store : isFixedGlobalAddr, is going to request | addr %p, typeLen %lu\n", addr, typeLen);
        }
        socket->pushWordF(STORE_REQ);
        socket->pushWordF(typeLen);
        
        uint32_t intAddr;
        memcpy(&intAddr, &addr, 4);

        socket->pushWordF(intAddr);
        socket->pushRangeF(&data, typeLen);
        socket->sendQue();
        LOG("[client] Store : sizeof(addr) %d, data length %d\n", sizeof(addr), typeLen);

        socket->receiveQue();
        int mode = socket->takeWord();
        LOG("[client] mode : %d\n", mode);
        assert(mode == STORE_REQ_ACK && "wrong");
        int len = socket->takeWord();
        if (len == 0) { // Normal ack
          //LOG("[client] TEST stored value : %d\n", *((int*)addr));
        } else if (len == -1) {
          LOG("[client] store request fail\n"); // TODO: have to handler failure situation.
        } else {
          assert(0 && "error: undefined behavior");
        }
        LOG("[client] Store : storeHandler END\n\n");
      }
    }

    void *UVAManager::memsetHandler(QSocket *socket, void *addr, int value, size_t num) {
      if (xmemIsHeapAddr(addr) || isFixedGlobalAddr(addr)) {
        LOG("[client] Memset : memsetHandler start (%p)\n", addr);
        if (xmemIsHeapAddr(addr)) {
          LOG("[client] Memset : isHeapAddr, is going to request | addr %p\n", addr);
        } else if (isFixedGlobalAddr(addr)) {
          LOG("[client] Memset : isFixedGlobalAddr, is going to request | addr %p\n", addr);
        }
        
        socket->pushWordF(MEMSET_REQ);
        uint32_t intAddr;
        memcpy(&intAddr, &addr, 4);
        socket->pushWordF(intAddr);
        socket->pushWordF(value);
        socket->pushWordF(num); // XXX check
        socket->sendQue();
        LOG("[client] Memset : memset(%p, %d, %d)\n", addr, value, num);

        socket->receiveQue();
        int ack = socket->takeWordF();
        assert(ack == MEMSET_REQ_ACK && "wrong");

        LOG("[client] Memset : memsetHandler END (%p)\n\n", addr);
      }
    }

    void *UVAManager::memcpyHandler(QSocket *socket, void *dest, void *src, size_t num) {
      if (xmemIsHeapAddr(dest) || isFixedGlobalAddr(dest)) {
        LOG("[client] Memcpy : memcpyHandler start (%p <- %p)\n", dest, src);
        if (xmemIsHeapAddr(dest)) {
          LOG("[client] Memcpy : isHeapAddr, is going to request | %p <- %p\n", dest, src);
        } else if (isFixedGlobalAddr(dest)) {
          LOG("[client] Memcpy : isFixedGlobalAddr, is going to request | %p <- %p\n", dest, src);
        }
        
        socket->pushWordF(MEMCPY_REQ);
        int intAddrDest;
        memcpy(&intAddrDest, &dest, 4);
        socket->pushWordF(intAddrDest);
        socket->pushWordF(num);
        socket->pushRangeF(src, num);
        //socket->pushWordF(num); // XXX check
        socket->sendQue();
        LOG("[client] Memcpy : memcpy(%p, %p, %d)\n", dest, src, num);
        //LOG("[client] Memcpy : src mem stat\n");
        //xmemDumpRange(src, num);
        hexdump("stack", src, num);
        socket->receiveQue();
        int ack = socket->takeWord();
        assert(ack == MEMCPY_REQ_ACK && "wrong");

        LOG("[client] Memcpy : memcpyHandler END (%p <- %p)\n\n", dest, src);
      }

    }

		/*** Get/Set Interfaces ***/
		void UVAManager::setConstantRange (void *begin_noconst, void *end_noconst/*, void *begin_const, void *end_const*/) { /* FIXME */
      ptNoConstBegin = truncToPageAddr (begin_noconst);
      ptNoConstEnd = truncToPageAddr ((void *)((XmemUintPtr)end_noconst + XMEM_PAGE_SIZE - 1));
			//ptConstBegin = truncToPageAddr (begin_const);
			//ptConstEnd = truncToPageAddr ((void *)((XmemUintPtr)end_const + XMEM_PAGE_SIZE - 1));

      printf("UVAManager::setConstantRange: ptNoConst (%p~%p) \n", ptNoConstBegin, ptNoConstEnd/*, ptConstBegin, ptConstEnd*/);
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

		/*** CallBack ***/
		void UVAManager::pageMappedCallBack (XmemUintPtr paddr) {
			/* set to MODIFIED/EXCLUSIVE state */
      printf("UVAManger::pageMappedCallBack: paddr (%p)\n", paddr);
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

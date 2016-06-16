/***
 * mm.cpp: Explicit Memory Manager
 *
 * Manages explicitly allocated pages
 * Provides customized heap management interfaces
 * XXX USES 32-BIT VIRTUAL MEMORY SPACE XXX
 * written by : gwangmu(polishing), hyunjoon
 *
 * **/

#include <cstdio> 
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <csignal>
#include "mm.h"
#include "chunk.h"
#include "mmapset.h"
#include "xmem_log.h"
#include "log.h"

#include "TimeUtil.h"
//#include "hexdump.h"

//#define DEBUG_UVA
//#define UVA_EVAL

namespace corelab {
	namespace XMemory {
		/*** Constants ***/
		static const int MAX_BIN_INDEX = 15;

		static const unsigned HSINFO_HEAPSIZE_OFF = 0;
		static const unsigned HSINFO_FREELIST_OFF = sizeof(uint32_t);
		static const unsigned HSINFO_FREESIZELIST_OFF =
			sizeof(uint32_t) + sizeof(UnivUintPtr) * (MAX_BIN_INDEX + 1);
		static const unsigned HSINFO_LASTCHUNK_OFF = 
			sizeof(uint32_t) * (MAX_BIN_INDEX + 2) +
			sizeof(UnivUintPtr) * (MAX_BIN_INDEX + 1);

		static const unsigned HSINFO_HEAPSIZE_SIZE = sizeof(uint32_t);
		static const unsigned HSINFO_FREELIST_SIZE =
			sizeof(UnivUintPtr) * (MAX_BIN_INDEX + 1);
		static const unsigned HSINFO_FREESIZELIST_SIZE =
			sizeof(uint32_t) * (MAX_BIN_INDEX + 1);
		static const unsigned HSINFO_LASTCHUNK_SIZE = sizeof(UnivUintPtr);

		static void * const HEAP_START_ADDR = (void *)0x18000000;
		static void * const HEAP_MAX_ADDR = (void *)0x38000000;

		static const unsigned EXPLICIT_PROT_MODE = PROT_READ | PROT_WRITE;

		/*** Declarations ***/
		/* (Variable) freeList 
				[bin index : size (bytes)]
				0 : ~64 		1 : ~128 		2 : ~256 		3 : ~512
				4 : ~1024 	5 : ~2048 	6 : ~4096  	7 : 4096~ */
		static mchunk freeList[MAX_BIN_INDEX + 1];
		static mchunk lastChunk;
		static uint32_t sizePrevHeap;				/**< the heap size when heap-state was just imported */
		static uint32_t sizeHeap;
		//static void *ptHeapTop = HEAP_START_ADDR;
		static uint32_t freeSizeList[MAX_BIN_INDEX + 1];
    static QSocket* socket;

		XMemoryManager::PageMappedCallBack pageMappedCallBack;

		/* (Variable) protAutoHeapMask
				Page protection mode mask.
				XORed with EXPLICIT_PROT_MODE to restore protection mode
				used when XMemoryManager automatically allocates pages */
		static unsigned protAutoHeapMask;

		static inline void registerSlab (size_t size);
		static inline void* allocatePage (void *addr, size_t size, unsigned protmode, bool mmap, bool isServer);
		static inline void deallocatePage (void *addr, size_t size);
		static inline void* askMoreMemory (size_t size, bool server);
		static inline mchunk getMoreMemory (size_t reqSize, uint64_t *binInx, bool server);
		static inline mchunk coalesce (mchunk head, mchunk tail);
		static inline size_t getBinIndex (size_t reqSize);
		static inline mchunk findChunk (size_t reqSize, uint64_t* binInx);
		static inline void useChunk (mchunk chunk, size_t reqSize, uint64_t binInx);

		static inline void removeFromFreeList(mchunk chunk, mchunk* head, uint64_t bininx);
		static inline void insertToFreeList(mchunk chunk, mchunk* head, uint64_t bininx);
		static inline int isZeroOrOneInList(mchunk* head);
		static inline void* truncToPageAddr (void *addr);



		/*** Initializer ***/
		void XMemoryManager::initialize (QSocket* Msocket) {
			// XXX DEPRECATED: MUST NEED TO BE INITIALIZED FOR NETWORK XXX
      
      socket = Msocket;
      assert(socket != NULL);

			//ptHeapTop = HEAP_START_ADDR;
			//protAutoHeap = PROT_READ | PROT_WRITE;
			//lastChunk = NULL;
		}	


		/*** Allocator/Deallocator ***/
		void* XMemoryManager::pagemap (void *addr, size_t size, bool isServer) {
      // don't use server because I don't send mmap information to server
			#ifdef XMEM_NO_APPLEVEL_MMAP
			if (addr < HEAP_START_ADDR) {
#ifdef DEBUG_UVA
				fprintf (stderr, "weird addr mapped (addr:%p)\n", addr);
#endif
				assert (0);
			}
			#endif

			return allocatePage (addr, size, EXPLICIT_PROT_MODE, true, isServer);
		}

		void* XMemoryManager::allocateServer (void *addr, size_t size) {
      // don't use server because I don't send mmap information to server
			#ifdef XMEM_NO_APPLEVEL_MMAP
			if (addr < HEAP_START_ADDR) {
#ifdef DEBUG_UVA
				fprintf (stderr, "weird addr mapped (addr:%p)\n", addr);
#endif
				assert (0);
			}
			#endif
      sizeHeap += size;
			return allocatePage (addr, size, EXPLICIT_PROT_MODE, false ,true);
		}

		void XMemoryManager::pageumap (void *addr, size_t size) {
			deallocatePage (addr, size);
		}

		void* XMemoryManager::allocate (size_t size, bool server) {
			// malloc (size);
			mchunk chunk;
			size_t alignedSize;  
			uint64_t binInx;

			if (size == 0) return NULL;
			
			alignedSize = getAlignedSize (size);
			alignedSize += UNIT_SIZE;

			registerSlab (alignedSize);
			chunk = findChunk (alignedSize, &binInx);
			if (chunk == NULL)
				chunk = getMoreMemory (alignedSize, &binInx, server);
			assert (chunk != NULL && "XMemoryManager::allocate() failed");
			useChunk (chunk, alignedSize, binInx);

			void* ret = chunk_getData (chunk);
			memset (ret, 0, size);

			return ret;
		}

		void* XMemoryManager::reallocate (void *addr, size_t size, bool server) {
			mchunk chunk;
			size_t alignedSize; 
			size_t orgSize;
			void *newAddr;

			if (addr) {
				chunk = chunk_getChunk(addr);
				orgSize = chunk_getSize(chunk);
				alignedSize = getAlignedSize(size);
				alignedSize += UNIT_SIZE;

				if (orgSize >= alignedSize) {
					newAddr = addr;
				}
				else {
					newAddr = allocate (size, server);
					memcpy(newAddr, addr, orgSize - UNIT_SIZE);
					free(addr);
				}
			}
			else {
				newAddr = allocate (size, server);
			}

			return newAddr;
		}

		void XMemoryManager::free (void *addr) {
			mchunk chunk, prev, next;
			uint64_t binInx;
			// assert(addr!=NULL);
			if(addr==NULL) return;

			assert (addr >= HEAP_START_ADDR && addr <= HEAP_MAX_ADDR && "weird address");

			chunk = chunk_getChunk(addr);
			registerSlab (chunk_getSize(chunk));

			// Coalesce chunk and prev in Mem
			prev = chunk_getPrevInMem(chunk);
			if(!chunk_isInUse(prev) ){
				binInx = chunk_getBinInx (prev);
				removeFromFreeList(prev, &freeList[binInx], binInx);
				chunk = coalesce(prev, chunk);
			}
			
			// Coalesce chunk and next in Mem
			next = chunk_getNextInMem(chunk);
			if(next <= lastChunk) {
				if(!chunk_isInUse(next)){
					binInx = chunk_getBinInx (next);
					removeFromFreeList(next, &freeList[binInx], binInx);
					chunk = coalesce(chunk, next);
				}
			}

			// chunk to freeList
			binInx = getBinIndex(chunk_getSize(chunk));

			insertToFreeList(chunk, &freeList[binInx], binInx);
			chunk_setFree(chunk);
			xmemLogAdd(CHUNK_DEL, chunk);
		}

		
		/*** Manipulator ***/
		void XMemoryManager::exportHeapState (HeapStateInfo &hsinfo) {
			memcpy (hsinfo.e + HSINFO_HEAPSIZE_OFF, &sizeHeap, HSINFO_HEAPSIZE_SIZE);

			//memcpy (hsinfo.e + HSINFO_FREELIST_OFF, freeList, HSINFO_FREELIST_SIZE);	
			UnivUintPtr *hsinfo_freeList = (UnivUintPtr *)(hsinfo.e + HSINFO_FREELIST_OFF);
			for (unsigned i = 0; i <= MAX_BIN_INDEX; i++) 
				hsinfo_freeList[i] = UNIV_UINT_PTR (freeList[i]);

			memcpy (hsinfo.e + HSINFO_FREESIZELIST_OFF, &freeSizeList, HSINFO_FREESIZELIST_SIZE);

			//memcpy (hsinfo.e + HSINFO_LASTCHUNK_OFF, &lastChunk, HSINFO_LASTCHUNK_SIZE);
			*(UnivUintPtr *)(hsinfo.e + HSINFO_LASTCHUNK_OFF) = UNIV_UINT_PTR (lastChunk);
		}

		void XMemoryManager::importHeapState (HeapStateInfo &hsinfo) {
			size_t sizeNHeap;
			memcpy (&sizeNHeap, hsinfo.e + HSINFO_HEAPSIZE_OFF, HSINFO_HEAPSIZE_SIZE);

			//if (sizeNHeap > sizeHeap) {
			//	size_t sizeAHeap = sizeNHeap - sizeHeap;
			//	allocatePage (getHeapTop (), sizeAHeap, EXPLICIT_PROT_MODE);
			//}
			sizeHeap = sizeNHeap;
			sizePrevHeap = sizeNHeap;

			//memcpy (freeList, hsinfo.e + HSINFO_FREELIST_OFF, HSINFO_FREELIST_SIZE);
			UnivUintPtr *hsinfo_freeList = (UnivUintPtr *)(hsinfo.e + HSINFO_FREELIST_OFF);
			for (unsigned i = 0; i <= MAX_BIN_INDEX; i++) 
				freeList[i] = (mchunk)hsinfo_freeList[i];

			memcpy (freeSizeList, hsinfo.e + HSINFO_FREESIZELIST_OFF, HSINFO_FREESIZELIST_SIZE);

			//memcpy (&lastChunk, hsinfo.e + HSINFO_LASTCHUNK_OFF, HSINFO_LASTCHUNK_SIZE);
			lastChunk = (mchunk)(*(UnivUintPtr *)(hsinfo.e + HSINFO_LASTCHUNK_OFF));
		}

		void XMemoryManager::clear () {
			// Set PROT_NONE protection, instead of MUNMAP
			// due to the performance issue
			for (UintPtr it = MmapSet::begin (); it != MmapSet::end (); it = MmapSet::next (it))
				mprotect ((void *)it, PAGE_SIZE, PROT_NONE);

			MmapSet::clear ();
		}


		/*** Test interfaces ***/
		bool XMemoryManager::isFreeHeapPage (void *paddr) {
			if (lastChunk == NULL) return false;
			if (HEAP_START_ADDR > paddr || paddr >= HEAP_MAX_ADDR) return false;

			UintPtr upaddr = (UintPtr)paddr;
			UintPtr heapEnd = (UintPtr)chunk_getNextInMem (lastChunk);
			if (upaddr >= heapEnd && upaddr < (UintPtr)getHeapTop ()) return true;

//		XXX DEPRICATED: freed pages contain chunk data
//			for (unsigned ibin = 6; ibin < MAX_BIN_INDEX + 1; ++ibin) {
//				for (mchunk freeChunk = freeList[ibin];
//						 freeChunk != NULL; freeChunk = chunk_getNextInList (freeChunk)) {
//					size_t chunkSize = chunk_getSize (freeChunk);
//					UintPtr chunkUAddr = (UintPtr)freeChunk;
//
//					UintPtr chunkAUAddr = (chunkUAddr + 0x00000FFF) & 0xFFFFF000;			
//					if (chunkSize < (chunkAUAddr - chunkUAddr)) continue;
//					chunkSize -= (chunkAUAddr - chunkUAddr);
//
//					if (upaddr < chunkAUAddr) continue;
//
//					for (UintPtr ufaddr = chunkAUAddr; chunkSize >= PAGE_SIZE;
//							 ufaddr += PAGE_SIZE, chunkSize -= PAGE_SIZE) {
//						if (ufaddr == upaddr)
//							return true;
//					}
//				}
//			}

			return false;
		}

		bool XMemoryManager::isMapped (void *paddr) {
			UintPtr uptPaddr = (UintPtr)truncToPageAddr (paddr);
			return (MmapSet::contains (uptPaddr));
		}

		bool XMemoryManager::isHeapAddr (void *addr) {
			return (HEAP_START_ADDR <= addr && addr < HEAP_MAX_ADDR);
		}

		/*** Get/Set interfaces ***/
		void* XMemoryManager::getHeapStartAddr () {
			return HEAP_START_ADDR;
		}

		void* XMemoryManager::getHeapMaxAddr () {
			return HEAP_MAX_ADDR;
		}

		size_t XMemoryManager::getPrevHeapSize () {
			return sizePrevHeap;
		}

		size_t XMemoryManager::getHeapSize () {
			return sizeHeap;
		}

    void* XMemoryManager::getHeapTop () {
			return (void *)((UintPtr)HEAP_START_ADDR + sizeHeap);
		}

		void XMemoryManager::setProtMode (void *addr, size_t size, unsigned protmode) {
			mprotect (addr, size, protmode);
		}

		/* @brief Defines page protection policy to use 
		 *	when XMemoryManager implicity allocates pages 
		 *	XXX DEPRECATED. DO NOT USE IT!!! XXX */
		void XMemoryManager::setAutoHeapPageProtPolicy (unsigned _protmode) {
			protAutoHeapMask = _protmode ^ EXPLICIT_PROT_MODE;
		}

		void XMemoryManager::setPageMappedCallBack (PageMappedCallBack handler) {
			pageMappedCallBack = handler;
		}

		UintPtr XMemoryManager::pageBegin () {
			return MmapSet::begin ();
		}

		UintPtr XMemoryManager::prevPageBegin () {
			if (sizePrevHeap < sizeHeap)
				return ((UintPtr)HEAP_START_ADDR + sizePrevHeap);
			else
				return MmapSet::end ();
		}

		UintPtr XMemoryManager::pageEnd () {
			return MmapSet::end ();
		}

		UintPtr XMemoryManager::pageNext (UintPtr addr) {
			return MmapSet::next (addr);
		}


		/*** Debug interface ***/
		void XMemoryManager::dumpRange (void *addr, size_t size) {
			UintPtr target = (UintPtr)addr;

			for(uint32_t i = 0; i < size; i+=4) {
				fprintf(stdout, "%p/%lu\n", (void *)target, *((UintPtr *)target));
				target += 4;
			}
		}

		void XMemoryManager::dumpFreeList () {
			for (int i = 0; i <= MAX_BIN_INDEX; ++i) {
				mchunk freeChunk;
				unsigned len = 0;
				unsigned maxSize = 0;

				freeChunk = freeList[i];
				while (freeChunk != NULL){
					unsigned chunkSize = chunk_getSize(freeChunk);
					maxSize = chunkSize > maxSize ? chunkSize : maxSize;
					freeChunk = chunk_getNextInList(freeChunk);
					len++;
				}

				fprintf (stderr, "freeList[%d] size:%u, len:%u, max:%u\n", i, freeSizeList[i], len, maxSize);
			}
			return;
		}

		/*** Internals ***/
		static inline void registerSlab (size_t size) {
			if (size >= PAGE_SIZE) return;

			// if slab already exists, stop.
			for (int i = 0; i < MAX_BIN_INDEX; i++) {
				if (freeSizeList[i] == size)
					return;
			}

			// if null slab exists, allocate slab.
			for (int i = 0; i < MAX_BIN_INDEX; i++) {
				if (freeSizeList[i] == 0) {
					freeSizeList[i] = size;
					return;
				}
			}

			// if no null slab but empty slab exists, overwrite slab.
			for (int i = 0; i < MAX_BIN_INDEX; i++) {
				if (freeList[i] == NULL) {
					freeSizeList[i] = size;
					return;
				}
			}
		}

		static inline void* allocatePage (void *addr, size_t size, unsigned protmode, bool isMmap, bool isServer) {

#ifdef UVA_EVAL
      StopWatch watch;
      if(!isServer) {
        watch.start();
      }
#endif
#ifdef DEBUG_UVA
      fprintf(stderr, "[mm] allocatePage: addr (%p) / size (%d) / protmode (%d) / isMmap (%d) / isServer (%d)\n", addr, size, protmode, isMmap, isServer);
#endif
      if(!isMmap && !isServer) {

        // [client side] just send size he want and get addr allocated
        socket->pushWordF(0);
        socket->pushWordF(4);
        socket->pushWordF(size);
        socket->sendQue();
        
        socket->receiveQue();
        int mode = socket->takeWord();
#ifdef DEBUG_UVA
        fprintf(stderr, "mode : %d\n", mode);
#endif
        //assert(mode == 1);
        int len = socket->takeWord();
#ifdef DEBUG_UVA
        fprintf(stderr, "len : %d\n", len);
#endif
        char buf[len];

        socket->takeRange(buf, len);
        memcpy(&addr, buf, len);
//#ifdef DEBUG_UVA
        fprintf(stderr, "[mm] client get a page with mapAddr : %p\n", addr);
//#endif


      } else if (isMmap && !isServer && protmode == 3) { /* XXX: is it safe ? */
#ifdef DEBUG_UVA
        printf("[mm] [client] mmap allocatePage\n");
#endif

        // [client side] just send size and addr
        socket->pushWordF(6); // mmap request mode
        uint32_t intAddr;
        memcpy(&intAddr, &addr, 4);
        socket->pushWordF(intAddr);
        socket->pushWordF(sizeof(size));
        socket->pushRangeF(&size, sizeof(size)); // send size
        socket->sendQue();
        
        socket->receiveQue();
        int mode = socket->takeWord();
#ifdef DEBUG_UVA
        fprintf(stderr, "mode : %d\n", mode);
#endif
        //assert(mode == 7);
        int ack = socket->takeWord();
        //assert(ack == 0);
        //fprintf(stderr, "len : %d\n", len);
        //char buf[len];

        //socket->takeRange(buf, len);
        //memcpy(&addr, buf, len);
        //fprintf(stderr, "mmapAddr : %p\n", addr);

      }

			void *res = mmap (addr, size, EXPLICIT_PROT_MODE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, (off_t)0);

			if (res != (void *)0xFFFFFFFF) {
				UintPtr _paddr = (UintPtr)truncToPageAddr (addr);
				size_t sizePages = size + (UintPtr)addr - _paddr;

				/* trigger upcall if a handler registered */
				for (; sizePages > 0; sizePages -= PAGE_SIZE,
						 _paddr += PAGE_SIZE) {
					MmapSet::insert (_paddr);
					if (pageMappedCallBack) pageMappedCallBack ((UintPtr)_paddr);
				}
			}
			else {
fprintf (stderr, "while allocating '%p'..\n", addr);
				perror ("mmap failed");
			}
#ifdef UVA_EVAL
      if(!isServer) {
        watch.end();
        FILE *fp = fopen("uva-eval.txt", "a");
        fprintf(fp, "MMAP %lf\n", watch.diff());
        fclose(fp);
      }
#endif
			return res;
		}

		static inline void deallocatePage (void *addr, size_t size) {
			/* Strictly speaking, this function doesn't really 'deallocate' page,
			 * but just erases the page address from MMAP_SET.
			 * This is just for coding convenience. */

			UintPtr _paddr = (UintPtr)truncToPageAddr (addr);
			size_t sizePages = size + (UintPtr)addr - _paddr;

			for (; sizePages > 0; sizePages -= PAGE_SIZE,
					 _paddr += PAGE_SIZE) {
				MmapSet::erase (_paddr);
			}
		}

		//@brief Return memory space when memory is not efficient in malloc
		static inline void* askMoreMemory (size_t size, bool server) {
			void *ptMem;
			void *res;

			ptMem = XMemoryManager::getHeapTop ();
			sizeHeap += size;
			res = allocatePage (ptMem, size, protAutoHeapMask ^ EXPLICIT_PROT_MODE, false, server);

			if(res == MAP_FAILED) {
				fprintf (stderr, "askMoreMemory failed: addr: %p size: %zu\n", ptMem, size);
				perror ("askMoreMemory failed to allocate memory");
				assert (0);
			}

// 			return ptMem;
      return res;
		}

		static inline mchunk getMoreMemory( size_t reqSize, uint64_t *binInx, bool server) {
			mchunk newChunk;
			//LOG(INFO, "reqSize: %lu \n", reqSize);
			reqSize = getAlignedPage(reqSize);
			newChunk = (mchunk)askMoreMemory(reqSize, server);
			if(lastChunk!=NULL) {
				if(chunk_getNextInMem(lastChunk)==newChunk) {
					if(chunk_isInUse(lastChunk)){
						chunk_setPrevSize(newChunk, chunk_getSize(lastChunk));
						xmemLogAdd(CHUNK_MOD, newChunk);
					} else {
						*binInx = getBinIndex(chunk_getSize(lastChunk));
						removeFromFreeList(lastChunk,&freeList[*binInx], *binInx);
						newChunk = coalesce(lastChunk, newChunk);
						reqSize += chunk_getSize(lastChunk);
					}
				} else {
					chunk_setPrevSize(newChunk, 0);
					xmemLogAdd(CHUNK_MOD, newChunk);
					lastChunk = newChunk;
				}
			} else {
				chunk_setPrevSize(newChunk, 0);
				xmemLogAdd(CHUNK_MOD, newChunk);
				lastChunk = newChunk;
			}
			chunk_setSize(newChunk, reqSize);
			xmemLogAdd(CHUNK_MOD, newChunk);

			insertToFreeList(newChunk, &freeList[MAX_BIN_INDEX], MAX_BIN_INDEX);
			*binInx = MAX_BIN_INDEX;

			return newChunk;
		}

		static inline mchunk coalesce (mchunk head, mchunk tail) {
			mchunk next;
			uint64_t size; 

			assert (chunk_getNextInMem (head) == tail && "head and tail is not connected");
			next = chunk_getNextInMem (tail);
			size = chunk_getSize (head) + chunk_getSize (tail);
			chunk_setSize(head, size);
			xmemLogAdd(CHUNK_MOD, head);
			if (next <= lastChunk) {
				chunk_setPrevSize (next, size);
				xmemLogAdd(CHUNK_MOD, next);
			}
			if (tail == lastChunk) lastChunk = head;
			return head;
		}

		static inline size_t getBinIndex (size_t reqSize) {
			for (int i = 0; i < MAX_BIN_INDEX; i++) {
				// if found slab, return it.
				if (freeSizeList[i] == reqSize)
					return i;
			}

			// if slab list fulled, return residue slab
			return MAX_BIN_INDEX;
		}

		static inline mchunk findChunk (size_t reqSize, uint64_t* binInx) {
			size_t i;
			i = getBinIndex (reqSize);

			// if empty normal slab or residue slab, find first-fit.
			if (i == MAX_BIN_INDEX || freeList[i] == NULL) {
				mchunk freeChunk = freeList[MAX_BIN_INDEX];
				while (freeChunk != NULL) {
					if (chunk_getSize (freeChunk) >= reqSize) {
						*binInx = MAX_BIN_INDEX;
						return freeChunk;
					}
					freeChunk = chunk_getNextInList (freeChunk);
				}
			}
			// if non-empty normal slab, return first one.
			else if (freeList[i] != NULL) {
				*binInx = i;

				return freeList[i];
			}

			return NULL;
		}

		static inline void useChunk(mchunk chunk, size_t reqSize, uint64_t binInx){
			size_t s;
			mchunk newChunk;
			mchunk next;
			s = chunk_getSize(chunk);
			removeFromFreeList(chunk, &freeList[binInx], binInx);

			/* split */
			if(s >= reqSize + MIN_CHUNK_SIZE){
				next = chunk_getNextInMem(chunk);
				newChunk = (mchunk) ((char *)chunk + reqSize);
				binInx = getBinIndex(s-reqSize);
				chunk_setFree(newChunk);
				xmemLogAdd(CHUNK_DEL, newChunk);
				chunk_setSize(newChunk,(s-reqSize));
				xmemLogAdd(CHUNK_MOD, newChunk);
				chunk_setSize(chunk, reqSize);
				xmemLogAdd(CHUNK_MOD, chunk);
				chunk_setPrevSize(newChunk, reqSize);
				xmemLogAdd(CHUNK_MOD, newChunk);

				if(next <= lastChunk) {
					chunk_setPrevSize(next, s-reqSize);
					xmemLogAdd(CHUNK_MOD, next);
				}

				insertToFreeList(newChunk, &freeList[binInx], binInx); 
				if(newChunk > lastChunk) lastChunk = newChunk;
			}

			chunk_setInUse(chunk);
			xmemLogAdd(CHUNK_NEW, chunk);
		}

	 

		static inline void removeFromFreeList(mchunk chunk, mchunk* head, uint64_t bininx) {
			mchunk next, prev;

			assert(chunk!=NULL);
			assert(head!=NULL);
if (chunk_getBinInx (chunk) != bininx)
	fprintf (stderr, "chunk_getBinInx (chunk): %lx, bininx: %lx\n", chunk_getBinInx (chunk), bininx);
			assert(chunk_getBinInx (chunk) == bininx);

			next = chunk_getNextInList(chunk);
			prev = chunk_getPrevInList(chunk);		

			if(chunk == *head) {
				*head = next;
				if(next!=NULL) 	{ 
					chunk_setPrevInList(next, NULL);
					xmemLogAdd(CHUNK_MOD, next);
				}
			} else {
				chunk_setNextInList(prev, next);
				xmemLogAdd(CHUNK_MOD, prev);
				if(next!=NULL)	{ 
					chunk_setPrevInList(next, prev);
					xmemLogAdd(CHUNK_MOD, next);
				}
			}

			chunk_setBinInx (chunk, (uint64_t)-1);
			xmemLogAdd(CHUNK_MOD, chunk);
		}

		static inline void insertToFreeList(mchunk chunk, mchunk* head, uint64_t bininx) {
			assert(chunk!=NULL);
			assert(head!=NULL);


			if(*head == NULL) {
				chunk_setPrevInList(chunk, NULL);
				xmemLogAdd(CHUNK_MOD, chunk);
				chunk_setNextInList(chunk, NULL);
				xmemLogAdd(CHUNK_MOD, chunk);
				*head = chunk;
			} else {
				chunk_setPrevInList(chunk, NULL);
				xmemLogAdd(CHUNK_MOD, chunk);
				chunk_setNextInList(chunk, *head);
				xmemLogAdd(CHUNK_MOD, chunk);
				chunk_setPrevInList(*head, chunk);
				xmemLogAdd(CHUNK_MOD, *head);
				*head = chunk;
			}

			chunk_setBinInx (chunk, bininx);
			xmemLogAdd(CHUNK_MOD, chunk);
		}

		// return 0 if nothing exists in the list
		// return 1 if only one element exists in the list
		// return 2 if more than one exist in the list
		static inline int isZeroOrOneInList(mchunk* head) {
			assert(head!=NULL);
			
			if(*head==NULL) return 0;
			if(chunk_getNextInList(*head) == NULL) return 1;
			return 2;
		}

		static inline void* truncToPageAddr (void *addr) {
			return (void *)((UintPtr)addr & PAGE_MASK);
		}
	}
}

/***
 * mm.cpp: Explicit Memory Manager
 *
 * Manages explicitly allocated pages
 * Provides customized heap management interfaces
 * XXX USES 32-BIT VIRTUAL MEMORY SPACE XXX
 * written by : gwangmu(polishing), hyunjoon
 *
 * **/

#ifndef CORELAB_XMEMORY_HEAP_MANAGER_H
#define CORELAB_XMEMORY_HEAP_MANAGER_H

#include <inttypes.h>
#include <sys/mman.h>
#include "memspec.h"
#include "qsocket.h"
		
using namespace std;

namespace corelab {
	namespace XMemory {
		// XXX USE HSINFO_SIZE rather than sizeof(HeapStateInfo)
		// XXX MUST BE CHANGED IF BIN SIZE CHANGED
		static const unsigned HSINFO_SIZE = 160;

		/* (Struct) HeapStateInfo
				Serialized heap state infomation for 
				exporting/importing use */
		struct HeapStateInfo {
			char e[HSINFO_SIZE];
		};

		namespace XMemoryManager {
			typedef void (*PageMappedCallBack) (UintPtr paddr);
      
			// Initializer
			void initialize (QSocket* Msocket);

			// Allocator/Deallocator
			void* pagemap (void *addr, size_t size);
			void* allocateServer (void *addr, size_t size);
			void pageumap (void *addr, size_t size);
			void* allocate (size_t size, bool server);
			void* reallocate (void *addr, size_t size, bool server);
			void free (void *addr);
			
			// Manipulator
			void exportHeapState (HeapStateInfo &hsinfo);
			void importHeapState (HeapStateInfo &hsinfo);
			void clear ();

			// Test interfaces
			bool isMapped (void *paddr);
			bool isFreeHeapPage (void *paddr);
			bool isHeapAddr (void *addr);

			// Get/set interfaces
			void* getHeapStartAddr ();
			void* getHeapMaxAddr ();
			size_t getPrevHeapSize ();
			size_t getHeapSize ();
      void * getHeapTop ();

			void setProtMode (void *addr, size_t size, unsigned protmode);
			void setAutoHeapPageProtPolicy (unsigned protmode);
			void setPageMappedCallBack (PageMappedCallBack handler);

			UintPtr pageBegin ();
			UintPtr prevPageBegin ();
			UintPtr pageEnd ();
			UintPtr pageNext (UintPtr addr);

			// Debug interface
			void dumpRange (void *addr, size_t size);
			void dumpFreeList ();
		}
	}
}

#endif

#ifndef __UVA_MANAGER_H__
#define __UVA_MANAGER_H__
/***
 * uva_manager.h: UVA Manager
 *
 * Runtime UVA manager
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/
#include <cstdlib>

#include "../comm/comm_manager.h"
#include "uva_comm_enum.h"
#include "qsocket.h"
#include "xmem_spec.h"

//using namespace std;

namespace corelab {
	namespace UVA {
		/* (Enum) UVAOwnership
				Defines UVA ownership.
				There is no operational difference between Master/Slave ownership.
				Be aware not to be misled by their literal meaning. */
    /* UVAOwnership is deprecated (BONGJUN) */
		enum UVAOwnership { OWN_MASTER, OWN_SLAVE };

    struct StoreLog {
      int32_t size;
      void *data;
      void *addr;
      StoreLog(int _size, void* _data, void* _addr) {
        size = _size;
        data = _data;
        addr = _addr;
      }
      ~StoreLog() {
        free(data);
      }
    };

		namespace UVAManager {
			void initialize (CommManager *comm, uint32_t destid);

#if 0 // GWANGMU LEGACY
			// UVA Management
			void synchIn (QSocket *socket);
			void synchOut (QSocket *socket);
			void fetchIn (QSocket *socket, void *addr);
			void fetchOut (QSocket *socket, void *addr);
			void flushIn (QSocket *socket);
			void flushOut (QSocket *socket);
			void resolveModified (void *addr);
#endif

      // synchronization for HLRC (Home-based Lazy Release Consistency)
      void acquireHandler_hlrc(CommManager *comm, uint32_t destid);
      void releaseHandler_hlrc(CommManager *comm, uint32_t destid);
      
      void syncHandler_sc(CommManager *comm, uint32_t destid);
      void syncHandler_hlrc(CommManager *comm, uint32_t destid);

      // Memory Access handler (BONGJUN)
      void loadHandler_sc(CommManager *comm, uint32_t destid, size_t typeLen, void *addr);
      void storeHandler_sc(CommManager *comm, uint32_t destid, size_t typeLen, void *data, void *addr);
      void storeHandler_hlrc(size_t typeLen, void *data, void *addr);
    
      void *memsetHandler_sc(CommManager *comm, uint32_t destid, void *addr, int value, size_t num);
      void *memsetHandler_hlrc(void *addr, int value, size_t num);
      void *memcpyHandler_sc(CommManager *comm, uint32_t destid, void *dest, void *src, size_t num);
      void *memcpyHandler_hlrc(CommManager *comm, uint32_t destid, void *dest, void *src, size_t num);
     

      // Get/Set/Test interfaces
			void setConstantRange (void *begin_noconst, void *end_noconst/*, void *begin_const, void *end_const*/);
      void getFixedGlobalAddrRange (void **begin_noconst, void **end_noconst/*, void **begin_const, void **end_const*/);
			size_t getHeapSize ();
			bool hasPage (void *addr);
      bool isFixedGlobalAddr (void *addr); // by BONGJUN

			// CallBack
			void pageMappedCallBack (XmemUintPtr paddr);
		}
	}
}
#endif

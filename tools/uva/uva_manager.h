/***
 * uva_manager.h: UVA Manager
 *
 * Runtime UVA manager
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

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

		namespace UVAManager {
			void initialize (QSocket *socket);

			// UVA Management
			void synchIn (QSocket *socket);
			void synchOut (QSocket *socket);
			void fetchIn (QSocket *socket, void *addr);
			void fetchOut (QSocket *socket, void *addr);
			void flushIn (QSocket *socket);
			void flushOut (QSocket *socket);
			void resolveModified (void *addr);

      // Memory Access handler (BONGJUN)
      void loadHandler(QSocket *socket, size_t typeLen, void *addr);
      void storeHandler(QSocket *socket, size_t typeLen, void *data, void *addr);
    
      void *memsetHandler(QSocket *socket, void *addr, int value, size_t num);
      void *memcpyHandler(QSocket *socket, void *dest, void *src, size_t num);
      
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

/***
 * uva_util.cpp: Native Offload Common Interfaces
 *
 * Offload common utility interfaces 
 * for both the server and the client.
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "uva_util.h"
#include "uva_manager.h"
#include "debug.h"

namespace corelab {
	namespace UVA {
/*		extern "C" void UVAUtilMemcpy (void* dest, void* source, uint32_t num) {
DEBUG_STMT (fprintf (stderr, "UVAUtilMemcpy: copying.. (dest: %p, source: %p, size: %u)\n", dest, source, num));
			memcpy (dest, source, num);
			return;
		}
*/
		extern "C"
    void UVAUtilSetConstantRange (void *begin_noconst, void *end_noconst/*, void *begin_const, void *end_const*/) {
fprintf (stderr, "setting constant range.. NoConst (begin: %p, end %p)\n", begin_noconst, end_noconst/*, begin_const, end_const*/);
			UVAManager::setConstantRange (begin_noconst, end_noconst/*, begin_const, end_const*/);
		}
	}
}

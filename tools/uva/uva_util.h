/***
 * uva_util.h: Native Offload(TM) Common Utility
 *
 * Offload utility APIs.
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#include <cstdlib>
#include <inttypes.h>

namespace corelab {
	namespace UVA {
		//extern "C" void offloadUtilMemcpy (void* dest, void* source, uint32_t num);
		extern "C" void UVAUtilSetConstantRange (void *begin_noconst, void *end_noconst/*, void *begin_const, void *end_const*/);
	}
}

/***
 * memspec.h: Memory Specification
 *
 * Common memory specification (32bit)
 * Provides common constants, types, auxiliary functions.
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_XMEMORY_MEMSPEC_H
#define CORELAB_XMEMORY_MEMSPEC_H

#include <set>
#include <inttypes.h>

#define PAGE_COUNT 		1048576
#define PAGE_SIZE 		4096
#define PAGE_BITS 		12
#define PAGE_MASK 		0xFFFFF000
#define PAGE_MASK_INV 0x00000FFF

#ifdef __x86_64__
#	define UNIV_UINT_PTR(x) ((UnivUintPtr)(uint64_t)x)
#else
#	define UNIV_UINT_PTR(x)	((UnivUintPtr)x)
#endif

namespace corelab {
	namespace XMemory {
		#ifdef __x86_64__
		typedef uint64_t UintPtr;				/** Arithmetic type for pointer <**/
		#else
		typedef uint32_t UintPtr;
		#endif

		typedef uint32_t* UnivUintPtr;		/** Universal arith. type of pt. <**/
	}
}

#endif

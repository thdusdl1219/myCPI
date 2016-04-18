/**
 * chunk.h: functions for supporting memory chunk management
 *
 *
 **/
#ifndef CHUNK_H
#define CHUNK_H

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include "memspec.h"

namespace corelab {
	namespace XMemory {
		struct malloc_str {
			// header fields
			uint64_t prev_size;		/* Size of previous chunk (if free).  */
			uint64_t head;				/* Size of this chunk & inuse bits. */
			uint64_t bininx;			/* gwangmu: bin index (if free) */

			// data fields
			UnivUintPtr fd;	/* double links -- used only if free. */
			UnivUintPtr bk;	/* (type == malloc_str*) */
		};

		typedef struct malloc_str mchunk_t;
		typedef struct malloc_str *mchunk;
	}
}

#ifdef __x86_64__
#	define MCHUNK(x) ((mchunk)(uint64_t)x)
#else
#	define MCHUNK(x) ((mchunk)x)
#endif

#define MIN_CHUNK_SIZE 48
#define MIN_SIZE_IN_BIN 72	
#define UNIT_SIZE 24 					/* Size of header (=fields before FD pointer) */

#define chunk_isInUse(chunk)  ((chunk)->head & 1U)
#define chunk_setInUse(chunk) ((chunk)->head |= 1U)
#define chunk_setFree(chunk)  ((chunk)->head &= ~1LU)

#define chunk_getSize(chunk)    ((chunk)->head & 0xFFFFFFFE)
#define chunk_setSize(chunk, s) ((chunk)->head = (s & (~1LU)))

#define chunk_getBinInx(chunk)    ((chunk)->bininx)
#define chunk_setBinInx(chunk, s) ((chunk)->bininx = s)

#define chunk_getOwner(chunk)    ((chunk)->head >> 32)
#define chunk_setOwner(chunk, s) ((chunk)->head |= (s<<32) )

#define chunk_getNextInList(chunk) MCHUNK((chunk)->fd)
#define chunk_getPrevInList(chunk) MCHUNK((chunk)->bk)
#define chunk_setNextInList(chunk, n) ((chunk)->fd = UNIV_UINT_PTR (n))
#define chunk_setPrevInList(chunk, p) ((chunk)->bk = UNIV_UINT_PTR (p))

#define chunk_setPrevSize(chunk, s) ((chunk)->prev_size = s)
#define chunk_getNextInMem(chunk)   ((mchunk) ((char *) (chunk) + chunk_getSize(chunk)))
#define chunk_getPrevInMem(chunk)   ((mchunk) ((char *) (chunk) - (chunk)->prev_size))
#define chunk_getData(chunk) ((void *)((char *)(chunk) + UNIT_SIZE))
#define chunk_getChunk(addr) ((mchunk)((char *)(addr) - UNIT_SIZE))

#endif

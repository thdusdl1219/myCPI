/***
 * mmapset.h : Mapped page set
 *
 * Set of pages mapped by process
 * XXX Must have no initialization XXX
 * XXX ONLY COMPATIBLE WITH 32-BIT SYSTEM XXX
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_XMEMORY_MMAP_SET_H
#define CORELAB_XMEMORY_MMAP_SET_H

#include "memspec.h"

namespace corelab {
	namespace XMemory {
		namespace MmapSet {
			// Manipulator
			void insert (UintPtr addr);
			void erase (UintPtr addr);
			void clear ();

			// Testing interface
			bool contains (UintPtr addr);

			// Iteration interfaces
			UintPtr begin ();
			UintPtr end ();
			UintPtr next (UintPtr cur);
		}
	}
}

#endif

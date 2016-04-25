/***
 * pageset.h : Page set
 *
 * Set of pages mapped by process
 * This is essentially 'class' version of mmapset.
 * XXX ONLY COMPATIBLE WITH 32-BIT SYSTEM XXX
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_OFFLOAD_PAGE_SET_H
#define CORELAB_OFFLOAD_PAGE_SET_H

#include "xmem_info.h"

#define L1_TABLE_LEN (XMEM_PAGE_COUNT / sizeof (BitVec))
#define L2_TABLE_LEN (L1_TABLE_LEN / sizeof (BitVec))

namespace corelab {
	namespace UVA {
		class PageSet {
		private:
			typedef uint32_t BitVec;

			BitVec tblL1[L1_TABLE_LEN];
			BitVec tblL2[L2_TABLE_LEN];

			XmemUintPtr minaddr;
			unsigned count;

			unsigned findNonZeroIdx (BitVec bvec, unsigned from);
			unsigned findNonZeroL2Entry (unsigned from);
			XmemUintPtr toXmemUintPtr (unsigned tag, unsigned idx);

		public:
			PageSet ();

			// Manipulator
			void insert (XmemUintPtr addr);
			void erase (XmemUintPtr addr);
			void clear ();

			// Testing interface
			unsigned size ();
			bool contains (XmemUintPtr addr);

			// Iteration interfaces
			XmemUintPtr begin ();
			XmemUintPtr end ();
			XmemUintPtr next (XmemUintPtr cur);
		};
	}
}

#endif

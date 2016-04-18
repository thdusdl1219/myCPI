/***
 * mmapset.c : Mapped page set
 *
 * Set of pages mapped by process
 * XXX Must have no initialization XXX
 * XXX ONLY COMPATIBLE WITH 32-BIT SYSTEM XXX
 * written by: gwangmu
 *
 * **/

#include <cstring>
#include <cassert>
#include <inttypes.h>
#include "mmapset.h"
#include <cstdio>

#define L1_TABLE_LEN (PAGE_COUNT / sizeof (BitVec))
#define L2_TABLE_LEN (L1_TABLE_LEN / sizeof (BitVec))

#define BITVEC_SIZE 32  	/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_BITS 5			/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_MASK 0x1F 	/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_SET_NTH(bvec, n) (bvec = bvec | (1 << n))
#define BITVEC_RESET_NTH(bvec, n) (bvec = bvec & ~(1 << n))
#define BITVEC_GET_NTH(bvec, n) ((bvec >> n) & 1)

#define MMAPSET_END 0

using namespace std;

namespace corelab {
	namespace XMemory {
		typedef uint32_t BitVec;

		static BitVec tblL1[L1_TABLE_LEN];
		static BitVec tblL2[L2_TABLE_LEN];

		static UintPtr minaddr;

		static inline unsigned findNonZeroIdx (BitVec bvec, unsigned from);
		static inline unsigned findNonZeroL2Entry (unsigned from);
		static inline UintPtr toUintPtr (unsigned tag, unsigned idx);

		// Manipulator
		void MmapSet::insert (UintPtr addr) {
			unsigned tag = addr >> PAGE_BITS;
			unsigned idx;

			addr = addr & PAGE_MASK;

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_SET_NTH (tblL1[tag], idx);

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_SET_NTH (tblL2[tag], idx);

			if (addr < minaddr || minaddr == MMAPSET_END) {
				minaddr = addr;
			}
		}

		void MmapSet::erase (UintPtr addr) {
			if (addr == minaddr) {
				minaddr = next (minaddr);
			}

			unsigned tag = addr >> PAGE_BITS;
			unsigned idx;

			addr = addr & PAGE_MASK;

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_RESET_NTH (tblL1[tag], idx);

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_RESET_NTH (tblL2[tag], idx);
		}

		void MmapSet::clear () {
			memset (tblL1, 0, sizeof (tblL1));
			memset (tblL2, 0, sizeof (tblL2));
		}

	
		// Testing Interface
		bool MmapSet::contains (UintPtr addr) {
			unsigned pgnum = addr >> PAGE_BITS;
			unsigned idx = pgnum & BITVEC_MASK;
			unsigned tag = pgnum >> BITVEC_BITS;

			return BITVEC_GET_NTH (tblL1[tag], idx);
		}


		// Iteration interfaces
		UintPtr MmapSet::begin () {
			return minaddr;
		}

		UintPtr MmapSet::end () {
			return MMAPSET_END;
		}

		UintPtr MmapSet::next (UintPtr cur) {
			unsigned pgnum = cur >> PAGE_BITS;
			unsigned tagL1 = pgnum >> BITVEC_BITS;
			unsigned idxL1 = pgnum & BITVEC_MASK;

			// find valid L1 index
			unsigned nidxL1 = findNonZeroIdx (tblL1[tagL1], idxL1 + 1);
		
			if (nidxL1 != (unsigned)-1) {
				return toUintPtr (tagL1, nidxL1);
			}
			else {
				// find valid L1 tag
				unsigned tagL2 = tagL1 >> BITVEC_BITS;
				unsigned idxL2 = tagL1 & BITVEC_MASK;

				unsigned ntagL1;
				unsigned nidxL2 = findNonZeroIdx (tblL2[tagL2], idxL2 + 1);
				
				if (nidxL2 != (unsigned)-1) {
					ntagL1 = (tagL2 << BITVEC_BITS) | nidxL2;

					// find valid L1 index (must success)
					nidxL1 = findNonZeroIdx (tblL1[ntagL1], 0);
					assert (nidxL1 != (unsigned)-1);

					return toUintPtr (ntagL1, nidxL1);
				}
				else {
					// find valid L2 tag
					unsigned ntagL2 = findNonZeroL2Entry (tagL2 + 1);

					if (ntagL2 != (unsigned)-1) {
						// find valid L1 tag (must success)
						nidxL2 = findNonZeroIdx (tblL2[ntagL2], 0);
						assert (nidxL2 != (unsigned)-1);
						ntagL1 = (ntagL2 << BITVEC_BITS) | nidxL2;

						// find valid L1 index (must success)
						nidxL1 = findNonZeroIdx (tblL1[ntagL1], 0);
						assert (nidxL1 != (unsigned)-1);

						return toUintPtr (ntagL1, nidxL1);
					}
					else {
						return MMAPSET_END;
					}
				}
			}
		}


		// Internals
		//@brief Finds non-zero index within bit-vector
		static inline unsigned findNonZeroIdx (BitVec bvec, unsigned from) {
			for (unsigned i = from; i < BITVEC_SIZE; i++) 
				if (BITVEC_GET_NTH (bvec, i))
					return i;
				
			return (unsigned)-1;
		}			

		//@brief Finds non-zero L2 entry within directory boundary.
		static inline unsigned findNonZeroL2Entry (unsigned from) {
			for (size_t i = from; i < L2_TABLE_LEN; i++)
				if (tblL2[i] != 0)
					return i;

			return (unsigned)-1;
		}

		static inline UintPtr toUintPtr (unsigned tag, unsigned idx) {
			return (UintPtr)(((tag << BITVEC_BITS) | idx) << PAGE_BITS);
		}
	}
}

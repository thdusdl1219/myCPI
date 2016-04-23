/***
 * pageset.cpp : Page set
 *
 * Set of pages mapped by process
 * This is essentially 'class' version of mmapset.
 *
 * Note: To make it free from constructor priority problem,
 * it doesn't let the constructor initialize memory,
 * rather requires a user to initialize by calling 'clear' manually.
 *
 * written by: gwangmu
 *
 * **/

#include <cstring>
#include <cassert>
#include <inttypes.h>
#include <cstdio>
#include "pageset.h"

#define BITVEC_SIZE 32  	/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_BITS 5			/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_MASK 0x1F 	/* XXX MUST BE CONSISTENT TO BITVEC TYPE XXX */
#define BITVEC_SET_NTH(bvec, n) (bvec = bvec | (1 << n))
#define BITVEC_RESET_NTH(bvec, n) (bvec = bvec & ~(1 << n))
#define BITVEC_GET_NTH(bvec, n) ((bvec >> n) & 1)

#define PAGESET_END 0

namespace corelab {
	namespace UVA {
		PageSet::PageSet () {
		}

		// Manipulator
		void PageSet::insert (XmemUintPtr addr) {
			unsigned tag = addr >> XMEM_PAGE_BITS;
			unsigned idx;

			addr = addr & XMEM_PAGE_MASK;

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			unsigned org = BITVEC_GET_NTH (tblL1[tag], idx);
			BITVEC_SET_NTH (tblL1[tag], idx);

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_SET_NTH (tblL2[tag], idx);

			if (addr < minaddr || minaddr == PAGESET_END) {
				minaddr = addr;
			}

			if (!org) count++;
fprintf (stderr, "inserted (count: %u)\n", count);
		}

		void PageSet::erase (XmemUintPtr addr) {
			if (addr == minaddr) {
				minaddr = next (minaddr);
			}

			unsigned tag = addr >> XMEM_PAGE_BITS;
			unsigned idx;

			addr = addr & XMEM_PAGE_MASK;

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			unsigned org = BITVEC_GET_NTH (tblL1[tag], idx);
			BITVEC_RESET_NTH (tblL1[tag], idx);

			idx = tag & BITVEC_MASK;
			tag = tag >> BITVEC_BITS;
			BITVEC_RESET_NTH (tblL2[tag], idx);

			if (org) count--;
fprintf (stderr, "erased (count: %u)\n", count);
		}

		void PageSet::clear () {
			memset (tblL1, 0, sizeof (tblL1));
			memset (tblL2, 0, sizeof (tblL2));

			minaddr = PAGESET_END;
			count = 0;
fprintf (stderr, "cleared (count: %u, &count: %p)\n", count, (void*)&count);
		}

	
		// Testing Interface
		bool PageSet::contains (XmemUintPtr addr) {
			unsigned pgnum = addr >> XMEM_PAGE_BITS;
			unsigned idx = pgnum & BITVEC_MASK;
			unsigned tag = pgnum >> BITVEC_BITS;

			return BITVEC_GET_NTH (tblL1[tag], idx);
		}

		unsigned PageSet::size () {
			return count;
		}


		// Iteration interfaces
		XmemUintPtr PageSet::begin () {
			return minaddr;
		}

		XmemUintPtr PageSet::end () {
			return PAGESET_END;
		}

		XmemUintPtr PageSet::next (XmemUintPtr cur) {
			unsigned pgnum = cur >> XMEM_PAGE_BITS;
			unsigned tagL1 = pgnum >> BITVEC_BITS;
			unsigned idxL1 = pgnum & BITVEC_MASK;

			// find valid L1 index
			unsigned nidxL1 = findNonZeroIdx (tblL1[tagL1], idxL1 + 1);
		
			if (nidxL1 != (unsigned)-1) {
				return toXmemUintPtr (tagL1, nidxL1);
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

					return toXmemUintPtr (ntagL1, nidxL1);
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

						return toXmemUintPtr (ntagL1, nidxL1);
					}
					else {
						return PAGESET_END;
					}
				}
			}
		}


		// Internals
		//@brief Finds non-zero index within bit-vector
		unsigned PageSet::findNonZeroIdx (BitVec bvec, unsigned from) {
			for (unsigned i = from; i < BITVEC_SIZE; i++) 
				if (BITVEC_GET_NTH (bvec, i))
					return i;
				
			return (unsigned)-1;
		}			

		//@brief Finds non-zero L2 entry within directory boundary.
		unsigned PageSet::findNonZeroL2Entry (unsigned from) {
			for (size_t i = from; i < L2_TABLE_LEN; i++)
				if (tblL2[i] != 0)
					return i;

			return (unsigned)-1;
		}

		XmemUintPtr PageSet::toXmemUintPtr (unsigned tag, unsigned idx) {
			return (XmemUintPtr)(((tag << BITVEC_BITS) | idx) << XMEM_PAGE_BITS);
		}
	}
}

/***
 * xmem_spec.h: XMemoryManager specification
 *
 * Memory specifications
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_XMEMORY_XMEM_SPEC_H
#define CORELAB_XMEMORY_XMEM_SPEC_H

#include <sys/mman.h>

/* XXX MUST BE CONSISTENT WITH OTHER DECLARATIONS */
#define XMEM_PAGE_COUNT 		1048576
#define XMEM_PAGE_SIZE 			4096
#define XMEM_PAGE_BITS 			12
#define XMEM_PAGE_MASK 			0xFFFFF000
#define XMEM_PAGE_MASK_INV 	0x00000FFF

#define XMEM_PROT_READ 			PROT_READ
#define XMEM_PROT_WRITE 		PROT_WRITE
#define XMEM_PROT_EXEC 			PROT_EXEC

/* XXX MUST BE CONSISTENT WITH OTHER DECLARATIONS */
#ifdef __x86_64__
typedef uint64_t XmemUintPtr;
#else
typedef uint32_t XmemUintPtr;
#endif

#endif

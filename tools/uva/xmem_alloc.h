/***
 * xmem_alloc.h: XMemoryManager (De)allocation Interface
 *
 * Declares (de)allocation interfaces
 * written by: gwangmu
 *
 * **/

#ifndef CORELAB_XMEMORY_XMEM_ALLOC_H
#define CORELAB_XMEMORY_XMEM_ALLOC_H

#include <stddef.h>
#include <string.h>

/* offload prefix for backward code compatibility */
extern "C" void* uva_malloc (size_t size);
extern "C" void uva_free (void *addr);
extern "C" void* uva_calloc (size_t nmemb, size_t size);
extern "C" void* uva_realloc (void* ptr, size_t size);

extern "C" void* uva_mmap (void *addr, size_t length, int prot, 
		int flags, int fd, off_t offset);

extern "C" char* uva_strdup (const char *str);

//extern "C" void* uva_memset(void* ptr,int value, size_t num);
//extern "C" void* uva_memcpy(void* dest, const void* source, size_t num);
//extern "C" void* uva_memmove(void* dest, const void* source, size_t num);

#endif

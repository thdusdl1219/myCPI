#include <assert.h>
#include <string.h> 
#include <cstdio>

#include "xmem_alloc.h"
#include "mm.h"
#include "log.h"

using namespace corelab::XMemory;

extern "C" void* uva_malloc (size_t size) {
	return XMemoryManager::allocate (size, false);
}

extern "C" void* uva_server_malloc (size_t size) {
  return XMemoryManager::allocate (size, true);
}

extern "C" void uva_free (void *addr) {
	XMemoryManager::free (addr);
}

extern "C" void* uva_calloc (size_t nmemb, size_t size) {
	return XMemoryManager::allocate (nmemb * size, false);
}

extern "C" void* uva_server_calloc (size_t nmemb, size_t size) {
	return XMemoryManager::allocate (nmemb * size, true);
}

extern "C" void* uva_realloc (void* addr, size_t size) {
	return XMemoryManager::reallocate (addr, size, false);
}

extern "C" void* uva_server_realloc (void* addr, size_t size) {
  return XMemoryManager::reallocate(addr, size, true);
}


extern "C" void* uva_mmap (void *addr, size_t length, int prot, 
		int flags, int fd, off_t offset) {
	assert (prot == 3 && "only read/writable pages can be allocated.");
	assert (flags == 50 && "only FIXED & ANONYMOUS & PRIVATE pages can be allocated.");
	assert (fd == -1 && "devices/files cannot be mmaped.");
	assert (offset == 0 && "no offset options allowed.");

  //printf("[xmem_alloc] uva_mmap called! addr (%p) / length (%d)\n", addr, length);
	return XMemoryManager::pagemap (addr, length, false);
}

extern "C" void* uva_server_mmap (void *addr, size_t length, int prot, 
		int flags, int fd, off_t offset) {
	assert (prot == 3 && "only read/writable pages can be allocated.");
	assert (flags == 50 && "only FIXED & ANONYMOUS & PRIVATE pages can be allocated.");
	assert (fd == -1 && "devices/files cannot be mmaped.");
	assert (offset == 0 && "no offset options allowed.");

	return XMemoryManager::pagemap (addr, length, false); /* XXX FIXME actually unused func */
}

extern "C" char* uva_strdup (const char *str) {
	char *npt = (char *)XMemoryManager::allocate ((strlen (str) + 1) * sizeof (char), false);
	strcpy (npt, str);

	return npt;
}

extern "C" char* uva_server_strdup (const char *str) {
	char *npt = (char *)XMemoryManager::allocate ((strlen (str) + 1) * sizeof (char), true);
	strcpy (npt, str);

	return npt;
}

/*extern "C" void* uva_memset(void* ptr, int value, size_t num){
	return memset(ptr,value,num);
}

extern "C" void* uva_memcpy(void* dest, const void* source, size_t num){
	return memcpy(dest,source,num);
}

extern "C" void* uva_memmove(void* dest, const void* source, size_t num){
	return uva_memmove(dest,source,num);
}*/

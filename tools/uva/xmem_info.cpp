#include <cstring>
#include "xmem_info.h"
#include "mm.h"

using namespace corelab::XMemory;

/* Page mapper */
extern "C" void* xmemPagemap (void *addr, size_t size) {
	return XMemoryManager::pagemap (addr, size);
}

extern "C" void xmemPageUnmap (void *addr, size_t size) {
	XMemoryManager::pageumap (addr, size);
}


/* Manipulator */
extern "C" void xmemExportState (XmemStateInfo *state) {
	HeapStateInfo hsinfo;
	XMemoryManager::exportHeapState (hsinfo);
	memcpy (state, &hsinfo, HSINFO_SIZE);
}

extern "C" void xmemImportState (XmemStateInfo state) {
	HeapStateInfo hsinfo;
	memcpy (&hsinfo, &state, HSINFO_SIZE);
	XMemoryManager::importHeapState (hsinfo);
}

extern "C" void xmemClear () {
	XMemoryManager::clear ();
}


/* Tester */
extern "C" int xmemIsMapped (void *paddr) {
	return (int)XMemoryManager::isMapped (paddr);
}

extern "C" int xmemIsFreeHeapPage (void *paddr) {
	return (int)XMemoryManager::isFreeHeapPage (paddr);
}

extern "C" int xmemIsHeapAddr (void *addr) {
	return (int)XMemoryManager::isHeapAddr (addr);
}


/* Getter/setter */
extern "C" void* xmemGetHeapStartAddr () {
	return XMemoryManager::getHeapStartAddr ();
}

extern "C" void* xmemGetHeapMaxAddr () {
	return XMemoryManager::getHeapMaxAddr ();
}

extern "C" size_t xmemGetPrevHeapSize () {
	return XMemoryManager::getPrevHeapSize ();
}

extern "C" size_t xmemGetHeapSize () {
	return XMemoryManager::getHeapSize ();
}

extern "C" void xmemSetProtMode (void *addr, size_t size, unsigned protmode) {
	return XMemoryManager::setProtMode (addr, size, protmode);
}

extern "C" void xmemSetAutoHeapPageProtPolicy (unsigned protmode) {
	XMemoryManager::setAutoHeapPageProtPolicy (protmode);
}

extern "C" void xmemSetPageMappedCallBack (XmemPageMappedCallBack handler) {
	XMemoryManager::setPageMappedCallBack (handler);
}


extern "C" XmemUintPtr xmemPageBegin () {
	return (XmemUintPtr)XMemoryManager::pageBegin ();
}

extern "C" XmemUintPtr xmemPrevPageBegin () {
	return (XmemUintPtr)XMemoryManager::prevPageBegin ();
}

extern "C" XmemUintPtr xmemPageEnd () {
	return (XmemUintPtr)XMemoryManager::pageEnd ();
}

extern "C" XmemUintPtr xmemPageNext (XmemUintPtr addr) {
	return (XmemUintPtr)XMemoryManager::pageNext ((UintPtr)addr);
}


/* Debugging interface */
extern "C" void xmemDumpRange (void *addr, size_t size) {
	XMemoryManager::dumpRange (addr, size);
}

extern "C" void xmemDumpFreeList () {
	XMemoryManager::dumpFreeList ();
}

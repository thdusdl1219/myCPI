/***
 * xmem_info.h: XMemoryManager Information Interface
 *
 * Declares XMemoryManager information interfaces
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#ifndef CORELAB_XMEMORY_XMEM_INFO_H
#define CORELAB_XMEMORY_XMEM_INFO_H

#include <stddef.h>
#include <inttypes.h>
#include "../comm/comm_manager.h"
#include "xmem_spec.h"
#include "qsocket.h" // BONGJUN

#define XSINFO_SIZE 	160

using namespace corelab;

/* XXX MUST BE CONSISTENT WITH OTHER DECLARATIONS */
typedef void (*XmemPageMappedCallBack) (XmemUintPtr);

struct XmemStateInfo {
	char e[XSINFO_SIZE];
};

/* initializer BONGJUN */
//extern "C" void xmemInitialize (QSocket *socket);
extern "C" void xmemInitialize (CommManager *comm, uint32_t destid);

/* Page mapper */
extern "C" void* xmemPagemap (void *addr, size_t size, bool isServer);
extern "C" void xmemPageUnmap (void *addr, size_t size);

/* Manipulator */
extern "C" void xmemExportState (XmemStateInfo *state);
extern "C" void xmemImportState (XmemStateInfo state);
extern "C" void xmemClear ();

/* Tester */
extern "C" int xmemIsMapped (void *paddr);
extern "C" int xmemIsFreeHeapPage (void *paddr);
extern "C" int xmemIsHeapAddr (void *addr);

/* Getter/setter */
extern "C" void* xmemGetHeapStartAddr ();
extern "C" void* xmemGetHeapMaxAddr ();
extern "C" size_t xmemGetHeapSize ();
extern "C" size_t xmemGetPrevHeapSize ();
extern "C" void xmemSetProtMode (void *addr, size_t size, unsigned protmode);
extern "C" void xmemSetAutoHeapPageProtPolicy (unsigned protmode);
extern "C" void xmemSetPageMappedCallBack (XmemPageMappedCallBack handler);

extern "C" XmemUintPtr xmemPageBegin ();
extern "C" XmemUintPtr xmemPrevPageBegin ();
extern "C" XmemUintPtr xmemPageEnd ();
extern "C" XmemUintPtr xmemPageNext (XmemUintPtr addr);

/* Debugging interface */
extern "C" void xmemDumpRange (void *addr, size_t size);
extern "C" void xmemDumpFreeList ();

#endif

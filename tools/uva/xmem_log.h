/***
 * xmem_log.h: XMemoryManager allocation logging Interface
 *
 * written by: hyunjoon
 *
 * **/

#ifndef CORELAB_XMEMORY_XMEM_LOG_H
#define CORELAB_XMEMORY_XMEM_LOG_H

#include <vector>
#include <stdint.h>
#include "chunk.h"

typedef enum ChunkModificationType{
	CHUNK_NEW = 1,
	CHUNK_MOD = 2,
	CHUNK_DEL = 3
} ChunkModificationType;

typedef struct XmemLog {
	ChunkModificationType type;
	void* addr;
	uint64_t id;
} XmemLog;

void xmemLogPrint();
void xmemLogAdd(ChunkModificationType type, void* addr);
uint64_t xmemLogRecentId();
std::vector<XmemLog>& getLogList();

#endif

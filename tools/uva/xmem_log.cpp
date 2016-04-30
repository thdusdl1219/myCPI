#include "xmem_log.h"
#include <stdio.h>
#include <cstdlib>
#include <vector>

#define XMEM_LOG_DEBUG

using namespace std;

static std::vector<XmemLog> logList;
static uint64_t totalId = 0;

void xmemLogAdd(ChunkModificationType type, void* addr) {
	XmemLog log;
	log.type = type;
	log.addr = addr;
	log.id = ++totalId;
//	logList.push_back(log);

#ifdef XMEM_LOG_DEBUG
	switch(log.type) {
		case CHUNK_NEW:
			fprintf(stderr, "chunk_new ");
			break;
		case CHUNK_MOD:
			fprintf(stderr, "chunk_mod ");
			break;
		case CHUNK_DEL:
			fprintf(stderr, "chink_del ");
			break;
	}
	fprintf(stderr, "addr - %p, size - %lu, id - %lu\n", log.addr, sizeof(corelab::XMemory::mchunk_t), log.id);
#endif
	return;
}

void xmemLogRemoveBefore(uint64_t id) {
	for(unsigned int i = 0; i < logList.size(); ++i) {
		XmemLog log = logList.front();
		if(log.id < id) logList.erase(logList.begin());
	}
	return;
}

void xmemLogPrint() {
	for(std::vector<XmemLog>::iterator it = logList.begin(); it != logList.end(); ++it) {
		XmemLog log = *it;
		switch(log.type) {
			case CHUNK_NEW:
				printf("chunk_new ");
				break;
			case CHUNK_MOD:
				printf("chunk_mod ");
				break;
			case CHUNK_DEL:
				printf("chink_del ");
				break;
		}
#ifdef XMEM_LOG_DEBUG
		printf("addr - %p, size - %lu, id - %lu\n", log.addr, sizeof(corelab::XMemory::mchunk_t), log.id);
#endif
	}
	return;
}

uint64_t xmemLogRecentId() {
	uint64_t recentId = 0;
	for(std::vector<XmemLog>::iterator it = logList.begin(); it != logList.end(); ++it) {
		XmemLog log = *it;
		if (log.id > recentId) recentId = log.id;
	}
	return recentId;
}

std::vector<XmemLog>& getLogList() {
	return logList;
}

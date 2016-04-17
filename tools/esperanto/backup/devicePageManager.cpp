#include <assert.h>
#include "devicePageManager.h"

namespace corelab {
	devicePageManagement::pages devicePageManagement::getPageId(void* address, size_t size) {
		uint64_t addrBegin = (uint64_t)(address);
		uint64_t addrEnd = addrBegin + size;
		unsigned int addrBeginIndex = addrBegin/pageSize;
		unsigned int addrEndIndex = addrEnd/pageSize;
		pages res = {addrBeginIndex, addrEndIndex - addrBeginIndex};
		return res;
	}

	void devicePageManagement::memoryRead(void* address, size_t size) {
#if 0
		pages res = getPageId(address, size);
		for (int pageId = res.start, i = 0; i < res.size; ++i) {
			pageInfo p = memoryOwnerShip[pageId];
			unsigned int localVersion = p.version;
			unsigned int ownership = p.version;

			if (ownership == 1 /*Owner*/) {
				/* do nothing */
			} else if (ownership == 2 /*Shared*/) {
				unsigned int currentVersion = getCurrentVersion(pageId);
				if (localVersion != currentVersion) {
					memoryOwnerShip[pageId].version = currentVersion; // local version update
					updateLocalPage(pageId);
				}
			} else if (ownership == 3 /*Modififed*/) {
				unsigned int currentVersion = getCurrentVersion(pageId);
				if (localVersion != currentVersion) {
					memoryOwnerShip[pageId].version = currentVersion; // local version update
					memoryOwnerShip[pageId].ownership = 2 /*Shared*/; // local ownwership update
					updateLocalPage(pageId);
				}
			} else {
				assert(0 && "unreachable code area");
			}
		}
#endif
		return;
	}

	void devicePageManagement::memoryWrite(void* address, size_t size) {
#if 0
		pages res = getPageId(address, size);
		for (int pageId = res.start, i = 0; i < res.size; ++i) {
			pageInfo p = memoryOwnerShip[pageId];
			unsigned int localVersion = p.version;
			unsigned int ownership = p.version;

			if (ownership == 1 /*Owner*/) {
				/* do nothing */
			} else if (ownership == 2 /*Shared*/) {
				memoryOwnerShip[pageId].version = localVersion; // local version update
				memoryOwnerShip[pageId].ownership = 1 /*Owner*/; // local ownwership update
				updateCurrentVersion(pageId, localVersion); 
				
			} else if (ownership == 3 /*Modififed*/) {
				memoryOwnerShip[pageId].version = localVersion; // local version update
				memoryOwnerShip[pageId].ownership = 1 /*Owner*/; // local ownwership update
				updateCurrentVersion(pageId, localVersion); 
			} else {
				assert(0 && "unreachable code area");
			}
		}
#endif
		return;
	}

	void devicePageManagement::memoryAllocation(void* address, size_t size) {
	}

	void devicePageManagement::memoryFree(void* address) {
	}
}

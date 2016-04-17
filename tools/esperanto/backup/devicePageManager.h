#include <stdint.h>
#include <map>

namespace corelab {
	using namespace std;

	class devicePageManagement {
		private:
			// ownersihp
			// 1: Owner
			// 2: Shared
			// 3: Modified
			typedef struct pageInfo {
				unsigned int version;
				unsigned int ownership;
			} pageInfo;

			// <page id, ownership>
			static std::map<unsigned int, pageInfo> memoryOwnerShip;
#if 0
			// <page id, ownership>
			std::map<unsigned int, unsigned int> allocationOwnerShip;
#endif
			
			// page id management
			typedef struct pages {
				unsigned int start;
				unsigned int size;
			} pages;
			static const uint32_t pageSize = 4096;
			static pages getPageId(void* address, size_t size);


			// TODO: return current version of pageId which is stored in gateway.
			static unsigned int getCurrentVersion(unsigned int pageId);
			// TODO: update local version of pageId into 
			static unsigned int updateCurrentVersion(unsigned int pageId, unsigned int version);

			// TODO: local page update as current page.
			// also other ownerships are changed
			static void updateLocalPage(unsigned int pageId);
			// TODO: current page update as local page.
			// also other ownerships are changed
			// it can be called in updateCurrentVersion();
			static void updateCurrentPage(unsigned int pageId);

			// TODO: listening thread for ownership changing or page request
			static void listenBroadcastFromGateway();


		public:
			static void memoryRead(void* address, size_t size);
			static void memoryWrite(void* address, size_t size);
			static void memoryAllocation(void* address, size_t size);
			static void memoryFree(void* address);
	};
}

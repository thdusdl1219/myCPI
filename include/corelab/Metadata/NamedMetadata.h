#ifndef LLVM_CORELAB_NAMEDMETADATA_H
#define LLVM_CORELAB_NAMEDMETADATA_H

#include "llvm/Pass.h"
#include "corelab/Metadata/Metadata.h"
#include <stdint.h>
#include <set>

namespace corelab { 
	using namespace llvm;

	class DeviceMapEntry{
		public:
			void setName(const char* name){
				sprintf(deviceName,"%s",name);
			}
			void setID(int id_){
				id = id_;
			}
			bool operator==(const DeviceMapEntry& source) const{
				return (strcmp(source.deviceName,deviceName) == 0);
			}
			bool operator!=(const DeviceMapEntry& source) const{
				return (strcmp(source.deviceName,deviceName) != 0);
			}
			bool operator<(const DeviceMapEntry& source) const{
				return (strcmp(source.deviceName,deviceName) != 0);
			}
			char deviceName[50];
			int id;
	};

	class DeviceMap{
		public:
			void insertEntry(DeviceMapEntry* entry){
				for(int i=0;i<(int)map.size();i++){
					if(strcmp(map[i]->deviceName,entry->deviceName) == 0){
						map[i]->id = entry->id;
						return;
					}
				}
				map.push_back(entry);
			}
			DeviceMapEntry* getEntry(DeviceMapEntry* source){
				for(int i=0;i<(int)map.size();i++){
					if(strcmp(map[i]->deviceName,source->deviceName) == 0){
						return map[i];
					}

				}
				return nullptr;
			}
			void print(){
				printf("PRINT---------------------------------\n");
				for(int i=0;i<(int)map.size();i++){
					printf("DEBUG :: device name : %s, device id : %d\n",map[i]->deviceName,map[i]->id);
				}
			}
		private:
			std::vector<DeviceMapEntry*> map;
	};

	class EsperantoNamer: public ModulePass
	{
		public:





			typedef enum EsperantoPlatform {
				X86 = 0,
				ARM = 1,
				AVR = 2
			} EsperantoPlatform;

			typedef struct DeviceEntry {
				char name[256];
				EsperantoPlatform platform;
			} DeviceEntry;
			
			typedef struct MetadataNode {
				DeviceEntry* dev;
				Instruction* inst;
			} MetadataNode;
			
			typedef struct FunctionTableEntry{
				int deviceID;
				char compname[256];
				char comptype[256];
			} FunctionTableEntry;
			
			static char ID;
			EsperantoNamer();

			const char *getPassName() const { return "EsperantoNamer"; }
			void getAnalysisUsage(AnalysisUsage &AU) const;
			std::string getClassNameInFunction(StringRef);
			bool runOnModule(Module &M);
			std::map<Function*,bool> remoteCallFunctionTable;
			std::map<int, FunctionTableEntry*> functionTable;
			std::map<CallInst*, DeviceEntry*> callMap;
			std::map<DeviceEntry*, uint32_t> deviceIdMap;
			DeviceMap deviceMap;
			int getIdCount(DeviceEntry* dev);
			int getIdCount(CallInst* call);
			
		private:
			Module* pM;
			EsperantoPlatform defaultPlatform;
			
			std::vector<DeviceEntry*> deviceList;
			std::vector<MetadataNode*> mdList;
			std::vector<MetadataNode*> mdListBuild;
			std::vector<Instruction*> alreadyChecked;
			std::vector<std::string> classNameList;
			std::vector<Function*> calledFunctionList;
			std::vector<Function*> remoteCallFunctions;

			void setMaps();
			
			void checkFunction();
			void buildClassNameList();
			void buildCalledFunctionList();
			void buildRemoteCalledClassList();
			void buildRemoteCallFunctionTable();
			void isExistOrInsert(std::string);
			void generateFunctionTableProfile();
						void checkCallInsts(Function* F, DeviceEntry* dev);
			bool isCheckedOrEnroll(Value* v);
			std::vector<Value*> checkedValueInFunction;
			
			void addMetadata();
			bool checkUseDefChains();
			void initUseDefChains();
			bool isCheckedOrEnroll(Instruction* I);
			void printSpecs();

			EsperantoPlatform getPlatform(const char* compType);
			const char* getPlatformAsString(EsperantoPlatform E);
			
			DeviceEntry* getDeviceInfo(Function* F);
			DeviceEntry* getOrInsertDeviceList(const char* compName, const char* compType = NULL);
	};
}

#endif

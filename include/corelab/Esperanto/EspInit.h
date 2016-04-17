#ifndef LLVM_ESP_INITIALIZER_H
#define LLVM_ESP_INITIALIZER_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"


namespace corelab
{
	using namespace llvm;
	using namespace std;

	struct MetadataInfo{
		StringRef* arg1;
		StringRef* arg2;
		StringRef* arg3;
	};

	struct DriverFunctionInfo{
		StringRef* driverName;
		StringRef* functionName;
		StringRef* condition;
	};

	struct DriverClassInfo{
		StringRef* driverName;
		StringRef* abstractClassName;
		StringRef* classCondition;
		struct DriverFunctionInfo* functions;
	};

	struct ProtocolInfo{
		StringRef* targetInit;
		StringRef* targetSend;
		StringRef* targetRecv;
		StringRef* targetFini;
	};

	struct RuntimeFunctions{
		StringRef* initHost;
		StringRef* initClnt;
		StringRef* send;
		StringRef* recv;
		StringRef* fini;
	};

	class ProtocolTableEntry{
		public:
			void insertHostProtocol(std::string name, struct ProtocolInfo info){
				hostProtocols[name] = info;
			}
			void insertClntProtocol(std::string name, struct ProtocolInfo info){
				clntProtocols[name] = info;
			}
			/*void insertHostProtocol(struct ProtocolInfo info){
				hostProtocols.push_back(info);
			}
			void insertClntProtocol(struct ProtocolInfo info){
				clntProtocols.push_back(info);
			}*/
			// BONGJUN
			std::map<std::string, struct ProtocolInfo> getHostProtocols(){
				return hostProtocols;
			}
			std::map<std::string, struct ProtocolInfo> getClntProtocols(){
				return clntProtocols;
			}
		private:
			//StringRef* deviceName;
			std::map<std::string, struct ProtocolInfo> hostProtocols;
			std::map<std::string, struct ProtocolInfo> clntProtocols;
	};

	class ProtocolTable{
		public:
			void insertProtocol(StringRef devName,ProtocolTableEntry entry){
				protocolTable[devName] = entry;
			}
			// BONGJUN
			std::map<StringRef, ProtocolTableEntry> getProtocolTable(){
				return protocolTable;
			}
		private:
			std::map<StringRef, ProtocolTableEntry> protocolTable; // device name & entry
	};

	class MetadataTable{
		public:
			void insertEspDevice(struct MetadataInfo mi){
				struct MetadataInfo* temp = new (struct MetadataInfo)();
				temp->arg1 = mi.arg1;
				temp->arg2 = mi.arg2;
				temp->arg3 = mi.arg3;
				EspDevice.push_back(temp);
				//printf("DEBUG :: EspDevice is inserted\n");
			}
			void insertEspDevDecl(struct MetadataInfo mi){
				struct MetadataInfo* temp = new (struct MetadataInfo)();
				temp->arg1 = mi.arg1;
				temp->arg2 = mi.arg2;
				temp->arg3 = mi.arg3;
				EspDevDecl.push_back(temp);
				//printf("DEBUG :: EspDevDecl is inserted\n");
			}
			void insertEspVarDecl(struct MetadataInfo mi){
				struct MetadataInfo* temp = new (struct MetadataInfo)();
				temp->arg1 = mi.arg1;
				temp->arg2 = mi.arg2;
				EspVarDecl.push_back(temp);
				//printf("DEBUG :: EspVarDecl is inserted\n");
			}
	
			std::vector<struct MetadataInfo*> getDevDeclList(){
				return EspDevDecl;
			}

			StringRef getDeviceName(StringRef type, StringRef name){
				//printf("DEBUG :: EspDevice size = %d\n",(int)EspDevice.size());
				for(int i=0;i<(int)EspDevice.size();i++){
					struct MetadataInfo* t = EspDevice[i];
					//printf("DEBUG :: loop1\n");
					if(strcmp(type.data(),t->arg1->data()) == 0 ){
						//printf("DEBUG :: strcmp -> %s / %s\n",type.data(),t->arg1->data());
						//printf("DEBUG :: strcmp -> %s / %s\n",name.data(),t->arg2->data());
						if(strcmp(name.data(),t->arg2->data()) == 0){
							//printf("DEBUG :: Output deviceName = %s\n",t->arg3->data());
							return (*(t->arg3));
						}
					}
				}
				return StringRef("");
			}

			StringRef getConstructorName(StringRef deviceName){
				for(int i=0;i<(int)EspDevDecl.size();i++){
					struct MetadataInfo* t = EspDevDecl[i];
					if(strcmp(deviceName.data(),t->arg1->data()) == 0){
						return (*(t->arg2));
					}
				}
				return StringRef("");
			}

			StringRef getDestructorName(StringRef deviceName){
				for(int i=0;i<(int)EspDevDecl.size();i++){
					struct MetadataInfo* t = EspDevDecl[i];
					if(strcmp(deviceName.data(),t->arg1->data()) == 0){
						return (*(t->arg3));
					}
				}
				return StringRef("");
			}

		private:
			std::vector<struct MetadataInfo*> EspDevice;
			std::vector<struct MetadataInfo*> EspDevDecl;
			std::vector<struct MetadataInfo*> EspVarDecl;
	};

	class DeviceInfoTable{
		public:
			int getDeviceID(StringRef deviceName){
				if(deviceInfoTable.find(deviceName) != deviceInfoTable.end()){
					return deviceInfoTable[deviceName];
				}
				return -1;
			}
			void insertDevice(StringRef deviceName){
				if(deviceInfoTable.find(deviceName) == deviceInfoTable.end()){
					deviceInfoTable[deviceName] = id;
					id++;
				}
			}
		private:
			int id = 0;
			std::map<StringRef,int> deviceInfoTable;
	};

	class FunctionTable{
		public:
			int getFunctionID(Function* function){
				if(functionTable.find(function) != functionTable.end()){
					return functionTable[function];
				}
				return -1;
			}
			void insertFunction(Function* function){
				if(functionTable.find(function) == functionTable.end()){
					functionTable[function] = functionCount;
					functionCount++;
				}
				else
					printf("DEBUG :: function is already exist\n");
			}
		private:
			int functionCount = 0;
			std::map<Function*,int> functionTable;
	};
	
	class EspInitializer : public ModulePass
	{
		public:

			bool runOnModule(Module& M);
			void buildMetadataTable();
			void buildDriverTable();
			void buildProtocolTable();
			void buildFunctionTable(Module& M);
			void buildProtocolTableImpl(std::map<StringRef,struct RuntimeFunctions>);	
			void makePTableEntry(ProtocolTableEntry* entry,std::map<StringRef,struct RuntimeFunctions>,StringRef);
			//void replaceBitCast(Module& M,Function*);
			//Function* setFunction(Module& M);
			//void checkCallInst(Module& M);

			//std::map<StringRef*,struct MetadataInfo> metadataTable;
			std::map<StringRef*,struct DriverClassInfo> driverTable;
			ProtocolTable PTable;	
			DeviceInfoTable DITable;
			MetadataTable MDTable;
			FunctionTable functionTable;
			//void getAnalysisUsage(AnalysisUsage&) const;
			const char *getPassName() const { return "EspInitializer"; }
			static char ID;
			EspInitializer() : ModulePass(ID) {}

			
	};
}

#endif



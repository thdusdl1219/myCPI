#ifndef LLVM_INST_MARKER_H
#define LLVM_INST_MARKER_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/Esperanto/EspInit.h"
namespace corelab
{
	using namespace llvm;
	using namespace std;

	/*struct MetadataInfo{
		StringRef* arg1;
		StringRef* arg2;
		StringRef* arg3;
	}MetadataInfo;

	struct DriverFunctionInfo{
		StringRef* driverName;
		StringRef* functionName;
		StringRef* condition;
	}DriverFunctionInfo;

	struct DriverClassInfo{
		StringRef* driverName;
		StringRef* abstractClassName;
		StringRef* classCondition;
		struct DriverFunctionInfo* functions;
	}DriverClassInfo;
*/
	typedef struct LocalFunctionInfo{
		bool isStatic;
		GlobalVariable* classPointer;
	}LocalFunctionInfo; 

	class LocalFunctionManager{
		public:
			void insertLocalFunction(Function* f, bool isStatic_, GlobalVariable* classPointer_){
				LocalFunctionInfo temp;
				temp.isStatic = isStatic_;
				temp.classPointer = classPointer_;
				localFunctions[f] = temp;
			}
			LocalFunctionInfo getLocalFunctionInfo(Function* f){
					return localFunctions[f];
			}
			bool isExist(Function* f){
				if(localFunctions.find(f) != localFunctions.end())
					return true;
				return false;
			}

		private:
			std::map<Function*,LocalFunctionInfo> localFunctions;
	};

	class InstMarker : public ModulePass
	{
		public:

			bool runOnModule(Module& M);
			void markInstruction();		
			void markClassInst(Module& M);	
			void markFunctionInst(Module& M);
			void removeMarkedRegion(Module& M);
			std::string getClassNameInFunction(StringRef functionName);
			StringRef getFunctionNameInFunction(StringRef functionName);
			void makeMetadata(Instruction* instruction, int Id);
			//std::map<StringRef*,struct MetadataInfo> metadataTable;
			//std::map<StringRef*,struct DriverClassInfo> driverTable;
			LocalFunctionManager LFManager;

			const char *getPassName() const { return "InstMarker"; }
			void getAnalysisUsage(AnalysisUsage& AU) const;
			static char ID;
			InstMarker() : ModulePass(ID) {}

	};
}

#endif


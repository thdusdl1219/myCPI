#ifndef LLVM_CORELAB_HOIST_VARIABLES_H
#define LLVM_CORELAB_HOIST_VARIABLES_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "corelab/Metadata/LoadNamer.h"
#include <set>

namespace corelab {
	using namespace llvm;
	using namespace std;
	typedef enum Mode {
		SERVER = 0,
		CLIENT = 1
	} Mode;

	class HoistVariables {
		public:
			HoistVariables(Module& M);
			void getGlobalVariableList(Module& M);

			void deployGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);
			void initializeGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);
			void freeHoistedGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);

			void hoistLocalVariable(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void hoistGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);

			void getOuterInstructions(Constant* c, set<Instruction*>& iset);
			void getOuterInstructions(Constant* c, set<Instruction*>& iset, set<GlobalVariable*>& gset);

			void createServerProduceGlobalVariable(Instruction* I);
			void createServerConsumeGlobalVariable(Instruction* I);
			void createClientProduceGlobalVariable(Instruction* I);
			void createClientConsumeGlobalVariable(Instruction* I);
			void createClientInitializeGlobalVariable(Instruction* I);
			void createServerInitializeGlobalVariable(Instruction* I);

			void createProduceFunction(Function* F, vector<GlobalVariable*>&, Mode mode, const DataLayout& dataLayout);
			void createConsumeFunction(Function* F, vector<GlobalVariable*>&, Mode mode, const DataLayout& dataLayout);
			void createGlobalVariableFunctions(const DataLayout& dataLayout);

			// selectively hoist
			bool findGlobalVariableInConstant(Constant* c, GlobalVariable* g);
			void checkHoistedInstCases(Instruction* I, GlobalVariable* gv, bool& isUsed);
			void distinguishGlobalVariables();
			void createSubGlobalVariables(Module& M, const DataLayout& dataLayout);
			
			GlobalVariable* findSubGlovar(GlobalVariable* oldGv);
			int findSubGlovarIndex(GlobalVariable* oldGv);
			void isCastNeeded(Constant* c, bool& castNeeded);
			
			Constant* unfoldZeroInitailizer(Constant* c);
			Value* castConstant(Constant* c, Instruction* I, int& loadNeeded, int& written, int& alloc, int i, const DataLayout& dataLayout);

			bool hasFunctionType (Type *type, set<Type *> setVisited = set<Type *> ());

			vector<GlobalVariable*> globalVar;						// all global

			// All global variables are included in Hoist, Constant_NoUse or NoConstant_NoUse
			// They are exclusive set.
			vector<GlobalVariable*> Hoist;                // behoisted
			vector<GlobalVariable*> Constant_NoUse;       // not be hoisted not be sent
			vector<GlobalVariable*> NoConstant_NoUse;     // not be hoisted but be sent

			vector<GlobalVariable*> InitNeeded;							// initailization target function
			vector<GlobalVariable*> subGlobalVar;         // mirror global for be hoisted
			vector<Value*> freeNeeded;										// allocated value during casting constnat
			vector<Value*> subGvAlloc;										// hoisted address on heap

		private:
			Constant* ClientInitiateGlobalVar;
			Constant* ServerInitiateGlobalVar;
			Constant* ServerProduceGlobalVar;
			Constant* ServerConsumeGlobalVar;
			Constant* ClientProduceGlobalVar;
			Constant* ClientConsumeGlobalVar;
			
			Constant* Malloc;
			Constant* Memcpy;
			Constant* Free;
			std::vector<Instruction*> removedCallInst;
			std::vector<Instruction*> substitutedCallInst;
	
			Function* ServerConsumeGlobalVariable;
			Function* ServerProduceGlobalVariable;
			Function* ClientConsumeGlobalVariable;
			Function* ClientProduceGlobalVariable;
			Function* ServerInitGlobalVariable;
			Function* ClientInitGlobalVariable;

			Constant* PrintAddress;
	};
}

#endif

#ifndef LLVM_CORELAB_HOIST_VARIABLES_H
#define LLVM_CORELAB_HOIST_VARIABLES_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include <set>

namespace corelab {
	using namespace llvm;
	using namespace std;

	class HoistVariables : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;
			
			static char ID;
			HoistVariables() : ModulePass(ID) {}
			const char *getPassName() const { return "HOIST_VARIABLES"; }
			
			void setFunctions(Module& M);
			void setIniFini(Module& M);

			void initializeHoistVariables(Module& M);
			void getGlobalVariableList(Module& M);

			void deployGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);
			void initializeGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);
			void freeHoistedGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);

			void hoistLocalVariable(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout);
			void hoistGlobalVariable(Module& M, Instruction* I, const DataLayout& dataLayout);

		private:
			void getOuterInstructions(Constant* c, std::set<Instruction*>& iset);
			void getOuterInstructions(Constant* c, set<Instruction*>& iset, set<GlobalVariable*>& gset);

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

			vector<GlobalVariable*> globalVar;						// all global

			// All global variables are included in Hoist, Constant_NoUse or NoConstant_NoUse
			// They are exclusive set.
			vector<GlobalVariable*> Hoist;                // behoisted
			vector<GlobalVariable*> Constant_NoUse;       // not be hoisted not be sent
			vector<GlobalVariable*> NoConstant_NoUse;     // not be hoisted but be sent

			vector<GlobalVariable*> InitNeeded;						// initailization target function
			vector<GlobalVariable*> subGlobalVar;         // mirror global for be hoisted
			vector<Value*> freeNeeded;										// allocated value during casting constnat
			vector<Value*> subGvAlloc;										// hoisted address on heap

			Constant* Malloc;
			Constant* Memcpy;
			Constant* Free;
			
			std::vector<Instruction*> removedCallInst;
			std::vector<Instruction*> substitutedCallInst;
	};
}

#endif

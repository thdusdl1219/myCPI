#ifndef LLVM_CORELAB_OFFLOAD_CLIENT_H
#define LLVM_CORELAB_OFFLOAD_CLIENT_H

#include <set>
#include <map>
#include <vector>
#include <string>

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"

#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/HoistVariables.h"
#include "corelab/UVA/FunctionPointerTranslater.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class OffloadClient : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			OffloadClient() : ModulePass(ID) {}
			const char *getPassName() const { return "OFFLOAD_CLIENT"; }

		private:
			Module *pM;
			LLVMContext *pC;
			LoadNamer *loadNamer;
			const DataLayout *dataLayout;

			Constant* cnstOffInit;
			Constant* cnstOffFinal;
			Constant* cnstOffProduceFID;
			Constant* cnstOffProduceFArgs;
			Constant* cnstOffConsumeRet;
			Constant* cnstOffRunFn;
			Constant* cnstOffWorth;

			/* Yielding feature extension */
			Constant* cnstOffConsumeYFID;
			Constant* cnstOffConsumeYFArgs;
			Constant* cnstOffRunYFn;
			Constant* cnstOffReturnYFn;
			Constant* cnstOffProduceYRet;
			
			HoistVariables* hoist;
			FunctionPointerTranslater* fcnptr;
			map<Function *, Function *> mapLocalToRemote;

			void initialize (Module &M);

			set<Function *> loadOffloadTargets ();
			void buildRemoteFunctions (set<Function *> setTargets);
			void installInitFinal ();
			void createExecYFunction ();
			
			void createProduceFID(Function* fn, Instruction* instBefore);
			void createProduceFArgs(Function* fn, vector<Value *> vecArgs, Instruction *instBefore);
			Instruction* createConsumeRet(Function* fn, Instruction* instBefore);
			
			void createProduceGlobalVariable(Instruction* instBefore);
			void createConsumeGlobalVariable(Instruction* instBefore);

			bool isOffLibOrLocalExtFn (string fnname);
	};
}

#endif

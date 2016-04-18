#ifndef LLVM_CORELAB_OFFLOAD_SERVER_H
#define LLVM_CORELAB_OFFLOAD_SERVER_H

#include <set>
#include <vector>

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

	class OffloadServer : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			OffloadServer() : ModulePass(ID) {}
			const char *getPassName() const { return "OFFLOAD_SERVER"; }

		private:
			Module *pM;
			LLVMContext *pC;
			LoadNamer *loadNamer;
			const DataLayout *dataLayout;

			set<string> setOffLibFns;
			set<string> setLocalExtFns;

			Constant* cnstOffStackInit;
			Constant* cnstOffExitFunc;
			Constant* cnstOffInit;
			Constant* cnstOffOpen;
			Constant* cnstOffFinal;
			Constant* cnstOffProduceRet;
			Constant* cnstOffConsumeFID;
			Constant* cnstOffConsumeFArgs;
			Constant* cnstOffRunFn;
			Constant* cnstOffReturnFn;

			Constant* cnstOffProduceYFID;
			Constant* cnstOffProduceYFArgs;
			Constant* cnstOffRunYFn;
			Constant* cnstOffConsumeYRet;

			HoistVariables* hoist;
			FunctionPointerTranslater* fcnptr;
			
			void initialize (Module &M);

			void installInitFinal ();
			void createExecFunction ();
			set<Function *> getNonOffLibAndLocalExtFns ();
			void buildYieldFunctions (set<Function *> fns);

			void createProduceGlobalVariable(Instruction* I);
			void createConsumeGlobalVariable(Instruction* I);	

			Instruction* createConsumeYRet (Function* fn, Instruction* instBefore);
			void createProduceYFArgs (Function* fn, vector<Value *> vecArgs, Instruction *instBefore);
			void createProduceYFID (Function* fn, Instruction* instBefore);

			bool isOffloadTarget(Function* F);
	};
}

#endif

#ifndef LLVM_CORELAB_ESPERANTO_SERVER_H
#define LLVM_CORELAB_ESPERANTO_SERVER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class EsperantoServer : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			EsperantoServer() : ModulePass(ID) {}
			const char *getPassName() const { return "ESPERANTO_SERVER"; }

		private:
			Function* initForCtr;
			Function* execFunction;

			Constant* Initialize;
			Constant* Finalize;
			Constant* IsSelfTarget;
			Constant* ProduceFunctionTarget;
			Constant* ProduceFunctionId;
			Constant* ProduceFunctionArg;

			void setFunctions(Module& M);
			void setIniFini(Module& M);
			void createExecFunction(Module& M);
			void setCalls(Module& M);
			void createCall(CallInst* C);

			std::vector<CallInst*> callLists;

			void createProduceFId(Function* f, Instruction* I);
			void createProduceFArgs(Function* f, Instruction* I, Instruction* insertBefore);
	};
}

#endif

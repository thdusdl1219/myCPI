#ifndef LLVM_CORELAB_ESPERANTO_FUNCTION_TABLE_H
#define LLVM_CORELAB_ESPERANTO_FUNCTION_TABLE_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class FunctionTable : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void init(Module& M);
			virtual void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			FunctionTable() : ModulePass(ID) {}
			
			const char *getPassName() const { return "FUNCTION_TABLE"; }

		private:
			Constant* FunctionTableInit;
			Constant* InsertFunctionTableComponent;
			
			//Function* initForCtr;
			//Function* execFunction;

			//Constant* Initialize;
			//Constant* Finalize;
			//Constant* IsSelfTarget;
			//Constant* ProduceFunctionTarget;
			//Constant* ProduceFunctionId;
			//Constant* ProduceFunctionArg;

			/*void setFunctions(Module& M);
			void setIniFini(Module& M);
			void createExecFunction(Module& M);
			void setCalls(Module& M);
			void createCall(CallInst* C);

			std::vector<CallInst*> callLists;

			void createProduceFId(Function* f, Instruction* I);
			void createProduceFArgs(Function* f, Instruction* I, Instruction* insertBefore);*/
	};
}

#endif

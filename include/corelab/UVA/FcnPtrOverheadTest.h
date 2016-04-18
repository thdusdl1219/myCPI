#ifndef LLVM_CORELAB_FCNPTR_OHDTEST_H
#define LLVM_CORELAB_FCNPTR_OHDTEST_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "corelab/UVA/HoistVariables.h"
#include "corelab/UVA/FunctionPointerTranslater.h"

namespace corelab {
	using namespace llvm;
	using namespace std;

	class FcnPtrOverheadTest : public ModulePass {
		public:
			bool runOnModule(Module& M);
			void getAnalysisUsage(AnalysisUsage &AU) const;

			static char ID;
			FcnPtrOverheadTest() : ModulePass(ID) {}
			const char *getPassName() const { return "FCNPTR_OHDTEST"; }

		private:
			void installRegistFcnPtr (Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout); 
	};
}

#endif

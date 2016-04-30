#ifndef LLVM_CORELAB_FIXED_GLOBAL_H
#define LLVM_CORELAB_FIXED_GLOBAL_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace std;

namespace corelab {
	class FixedGlobal : public ModulePass {
		public:
			static char ID;

			FixedGlobal () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "FIXED_GLOBAL"; }

		private:
			Module *pM;
      bool isFixGlbDuty;

			size_t convertToFixedGlobals (vector<GlobalVariable *> vecGvars, void *base);
      void findGV(Module &M, char *gvar_str, vector<GlobalVariable*> &vecGvars);
			bool hasFunction (Constant *cnst);
	};
}

#endif

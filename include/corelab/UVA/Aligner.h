#ifndef LLVM_CORELAB_ALIGNER_INSTALLER_H
#define LLVM_CORELAB_ALIGNER_INSTALLER_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <set>
#include <string>

#define OUT
#define UNUSED

using namespace llvm;
using namespace std;

namespace corelab {
	class AlignerInstaller : public ModulePass {
	public:
		static char ID;

		AlignerInstaller () : ModulePass (ID) {}

		void getAnalysisUsage (AnalysisUsage &AU) const;
		const char* getPassName () const { return "ALIGNER_INSTALLER"; }

		// Pass callback
		bool runOnModule (Module& M);

	private:
		Module *pM;
		LLVMContext *pC;

		void moveBody (Function *dst, Function *src);
		void redirectToAligner (Function *target, Function *link);
	};
}

#endif

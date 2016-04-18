#ifndef LLVM_CORELAB_HOT_ROUTINE_H
#define LLVM_CORELAB_HOT_ROUTINE_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/LoopInfo.h"

#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/Common.h"

#include <map>
#include <vector>
#include <string>

#define OUT
#define UNUSED

using namespace llvm;
using namespace std;
using namespace corelab::UVA;

namespace corelab {
	class HotRoutine : public ModulePass {
	public:
		static char ID;

		HotRoutine () : ModulePass (ID) {}

		void getAnalysisUsage (AnalysisUsage &AU) const;
		const char* getPassName () const { return "HOT_ROUTINE"; }

		// Pass callback
		bool runOnModule (Module& M);

	private:
		Module *pM;
		LLVMContext *pC;

		Type *tyVoid;
		Type *tyID;
	
		LoadNamer *loadNamer;

		void installInitFinal ();
		void installFunctionEvent ();
		void installLoopEvent ();

		// helper method
		ConstantInt* getConstantInt (unsigned bits, unsigned n);
		map<LoopID, Loop *> getTotalLoopsInFn (Function *fn);
		vector<Loop *> flattenSubLoopList (Loop *loop);
	};
}

#endif

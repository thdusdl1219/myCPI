#ifndef LLVM_CORELAB_DYN_LOADSTORE_H
#define LLVM_CORELAB_DYN_LOADSTORE_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>

#define OUT

using namespace llvm;
using namespace std;

namespace corelab {
	class DynLoadStore : public ModulePass {
		public:
			static char ID;

			DynLoadStore () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "DYN_LOADSTORE"; }
	};
}

#endif

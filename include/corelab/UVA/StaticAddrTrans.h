#ifndef LLVM_CORELAB_STATIC_ADDRTRANS_H
#define LLVM_CORELAB_STATIC_ADDRTRANS_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>

#define OUT

using namespace llvm;
using namespace std;

namespace corelab {
	class StaticAddrTrans : public ModulePass {
		public:
			static char ID;

			StaticAddrTrans () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "STATIC_ADDRTRANS"; }

		private:
			Module *pM;
			LLVMContext *pC;

			void convertStructMemberPtrToI32 ();
			void installAddrTranslator ();
	};
}

#endif

#ifndef LLVM_CORELAB_PORTING32_H
#define LLVM_CORELAB_PORTING32_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <map>

#define OUT

using namespace llvm;
using namespace std;

namespace corelab {
	class Converter32 : public ModulePass {
		public:
			static char ID;

			Converter32 () : ModulePass (ID) {}

			bool runOnModule (Module& M);
			void getAnalysisUsage (AnalysisUsage &AU) const;
			const char* getPassName () const { return "CONVERTER32"; }

		private:
			Module 			*pM;
			LLVMContext *pC;

			void convertTargetTriple ();	

			//helper function
			ConstantInt* getConstantInt (unsigned bits, unsigned n);
	};
}

#endif

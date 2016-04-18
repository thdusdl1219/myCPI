/***
 *
 * Offload/Coverage.h
 *
 * Check the coverage.
 *
 */

#ifndef LLVM_CORELAB_HOT_FUNCTION_H
#define LLVM_CORELAB_HOT_FUNCTION_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

#include <cstdio>

namespace corelab {
	using namespace llvm;

	class Coverage : public ModulePass {
		public:
			static char ID;
			Coverage() : ModulePass(ID) {}
			
			
			const char *getPassName() const { return "Coverage"; }

			virtual bool runOnModule (Module& M);
			virtual void getAnalysisUsage(AnalysisUsage& AU) const;
			bool runOnFunction(Function& F);

		private:
			Constant* beginFunction;
			Constant* endFunction;

			Constant* Initialize;
			Constant* Finalize;

			std::vector<uint32_t> offloadable;
			void getOffloadable();

			void setIniFini(Module& M);
			void setFunctions(Module& M);
	};
}

#endif

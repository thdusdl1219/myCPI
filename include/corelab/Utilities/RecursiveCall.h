/***
 * corelab/Utilities/RecursiveCall.h
 *
 * This module pass finds recursive call graph. Making the SCC of call graph,
 * find the each groups which contians the recursive call relationships.
 *
 * */

#ifndef LLVM_CORELAB_RECURSIVECALL_H
#define LLVM_CORELAB_RECURSIVECALL_H

#include "llvm/Pass.h"
#include "typedefs.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Profilers/campCommon.h"

namespace corelab{
	using namespace llvm;
	using namespace std;
	using namespace corelab::CAMP;

	class RecursiveCall: public ModulePass
	{
		public:
			static char ID;
			RecursiveCall();

			const char *getPassName() const { return "RecursiveCall"; }

			virtual void getAnalysisUsage(AnalysisUsage &AU) const
			{
				AU.addRequired< LoadNamer >();
				AU.addRequired< CallGraph >();
				AU.setPreservesAll();
			}

			bool runOnModule(Module &M);
			bool isRecursiveCall(FuncID calling, FuncID called);
			void printAll();
		private:
			typedef vector<FuncID>* funcArray;
			funcArray *recursiveGraph;
	};
}

#endif

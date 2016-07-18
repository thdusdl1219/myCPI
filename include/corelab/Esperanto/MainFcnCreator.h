#ifndef LLVM_MAIN_FCN_CREATOR_H
#define LLVM_MAIN_FCN_CREATOR_H

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "corelab/Esperanto/EspInit.h"
namespace corelab
{
	using namespace llvm;
	using namespace std;

	class MainCreator : public ModulePass
	{
		public:

			bool runOnModule(Module& M);
			void setFunctions(Module& M);
			void setIniFini(Module& M);
			Function* getMainFcn(Module& M);
			std::string getRealNameofFunction(StringRef fName);
      Function* getSimilarFunction(Module& M, std::string name);
      void removeOtherCtorDtor(Module& M);
			
			const char *getPassName() const { return "MainCreator"; }
			void getAnalysisUsage(AnalysisUsage& AU) const;
			static char ID;
			MainCreator() : ModulePass(ID) {}

      std::vector<Function*> functionToRemove;

		private:
			Function* constructor;
			Function* destructor;
      Function* mainFini;
			Function* mainFcn;

	};
}

#endif


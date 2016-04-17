#ifndef LLVM_CORELAB_REGION_TO_FUNCTION_H
#define LLVM_CORELAB_REGION_TO_FUNCTION_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class RegionToFunction : public ModulePass {
    public:
      static char ID;

      RegionToFunction () : ModulePass (ID) {}

      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "Region To Function"; }

      bool runOnModule (Module& M);
      Function *makeLoopFunction(Loop *loop, DominatorTree &dt);
  };
}
#endif

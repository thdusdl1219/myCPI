#ifndef LLVM_CORELAB_SWITCHCASE_TEST_H
#define LLVM_CORELAB_SWITCHCASE_TEST_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class SwitchCaseTest : public ModulePass {
    public:
      static char ID;

      SwitchCaseTest () : ModulePass (ID) {}

      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "SWITCHCASE_TEST"; }

      bool runOnModule (Module& M);
      void setFunctions (Module& M);
      Function* createExecFunction (Module& M);
   private:
      Function* Consume;
      Function* Produce;
      Function* Init;
      Function* Debug;
  };
}
#endif

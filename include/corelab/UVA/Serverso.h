#ifndef LLVM_CORELAB_UVA_SERVER_H
#define LLVM_CORELAB_UVA_SERVER_H


#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class UVAServer : public ModulePass {
    public:
      static char ID;

      UVAServer() : ModulePass (ID) {}
      
      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "UVA_SERVER"; }


      bool runOnModule(Module &M);

    private:
      Constant* UVAServerInit;
      Constant* UVAServerFinal;
      
      void setFunctions(Module &M);
      void setIniFini(Module &M);
  };
}

#endif

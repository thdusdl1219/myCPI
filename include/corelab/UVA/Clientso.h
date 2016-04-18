#ifndef LLVM_CORELAB_UVA_CLIENT_H
#define LLVM_CORELAB_UVA_CLIENT_H


#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

#include "corelab/UVA/HoistVariables.h"
//#include "corelab/UVA/FunctionPointerTranslater.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class UVAClient : public ModulePass {
    public:
      static char ID;

      UVAClient() : ModulePass (ID) {}
      
      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "UVA_Client"; }


      bool runOnModule(Module &M);

    private:
      Constant* UVAClientInit;
      Constant* UVAClientFinal;
      
      HoistVariables* hoist;
      //FunctionPointerTranslater* fcnptr;

      void setFunctions(Module &M);
      void setIniFini(Module &M);
  };
}

#endif

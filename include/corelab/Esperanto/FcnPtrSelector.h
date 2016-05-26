#ifndef LLVM_CORELAB_FCNPTR_SELECTOR_H
#define LLVM_CORELAB_FCNPTR_SELECTOR_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  class DriverImplInfo {
  public:
    DriverImplInfo(string rawname, vector<string> cond) {
      this->rawname = rawname;
      this->ptr = NULL;
      this->cond = cond;
    }
    /*DriverImplInfo(const DriverImplInfo &obj) {
      this->rawname = obj.rawname;
      this->ptr = obj.ptr;
      this->cond = obj.cond;
    }*/
    void setFunction(Function* ptr) {
      this->ptr = ptr;
    }
    string rawname;
    Function *ptr;
    vector<string> cond;
  };

  typedef pair<string, string> DriverKeyType;
  typedef map<pair<string, string>,string> DriverDeclMapType;
  typedef multimap<pair<string, string>, DriverImplInfo> DriverImplMapType;
  
  class FcnPtrSelector : public ModulePass {
    public:
      static char ID;

      FcnPtrSelector () : ModulePass (ID) {}

      void getAnalysisUsage (AnalysisUsage &AU) const override;
      const char* getPassName () const override { return "Function Pointer Selector Pass"; }

      bool runOnModule (Module &M) override;

      void parseDriverMetadata(Module &M);
      void setFunctionPointer(Module &M);
      void makeFcnPtrSelector(Module &M, pair<string, string> DeclInfo);

      static Value* EmitCallToStrcmp(Module& M, 
          Value *Arg, string str, BasicBlock *end);
    private:
      void parseCondition(vector<string> &emptyMap, string cond);
      multimap<pair<string, string>, DriverImplInfo> DriverImplMap;
  };
}
#endif

#ifndef LLVM_CORELAB_CPI_H
#define LLVM_CORELAB_CPI_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"


namespace corelab {
  using namespace llvm;

  struct CPIFunctions {
    Function *CPIInit;
    Function *CPISet; 
    Function *CPIGet; 
  };

  class CPIPre : public ModulePass {
    const DataLayout *DL;
  public:
    static char ID;

    CPIPre() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const {
    }

    bool doInitialization(Module &M) {
      DL = &M.getDataLayout();
      return false;
    }

    bool runOnModule(Module &M);
  };

  class CPI : public ModulePass {
    const DataLayout *DL;
    TargetLibraryInfo *TLI;
    AliasAnalysis *AA;

    CPIFunctions CF;

    DenseMap<StructType*, MDNode*> StructsTBAA;
    DenseMap<StructType*, MDNode*> UnionsTBAA;
    DenseMap<Function*, bool> CalledExternally;

    bool externallyCalled(Function* F);
    bool protectType(Type*, bool, MDNode* TBAATag = NULL);
    bool protectLoc(Value*, bool);
    bool protectValue(Value*, bool, MDNode* TBAATag = NULL);
    bool pointsToVTable(Value*);
    void insertChecks(DenseMap<Value*, Value*> &BM, Value *V, bool IsDereferenced, SetVector<std::pair<Instruction*, Instruction*> > &ReplMap);
    Function* createGlobalsReload(Module &M, StringRef N);
  public:
    static char ID;
    CPI() : ModulePass(ID) {}

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
      AU.addRequired<AAResultsWrapperPass>();
      AU.addRequired<TargetLibraryInfoWrapperPass>();
    }

    bool doInitialization(Module &M);
    bool doFinalization(Module &M);
    bool runOnFunction(Function &F);
    bool runOnModule(Module &M);
  };

}

#endif

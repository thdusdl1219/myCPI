#ifndef LLVM_CORELAB_MODULE_LOOPS_H
#define LLVM_CORELAB_MODULE_LOOPS_H

#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include "corelab/Utilities/GimmeLoops.h"

#include <map>
#include <stdio.h>

namespace llvm
{
  class DominatorTree;
  class PostDominatorTree;
  class LoopInfo;
  class ScalarEvolution;
}

namespace corelab
{
using namespace llvm;

struct ModuleLoops : public ModulePass
{
  static char ID;
  ModuleLoops() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &au) const
  { 
    //this line was commented out  originally//juhyun
    au.addRequired< TargetLibraryInfoWrapperPass >();
    au.setPreservesAll();
  }

  bool runOnModule(Module &mod)
  {
    //this line was commented out originally //juhyun
    td = &mod.getDataLayout();//juhyun
    tli = &getAnalysis< TargetLibraryInfoWrapperPass >().getTLI();
    return false;
  }

  void reset() { results.clear(); }
  void forget(Function *fcn) { results.erase(fcn); }

  DominatorTree &getAnalysis_DominatorTree(const Function *fcn);
  PostDominatorTree &getAnalysis_PostDominatorTree(const Function *fcn);
  LoopInfo &getAnalysis_LoopInfo(const Function *fcn);
  ScalarEvolution &getAnalysis_ScalarEvolution(const Function *fcn);

private:
  const DataLayout *td;
  TargetLibraryInfo *tli;
  std::map<const Function*, GimmeLoops> results;

  GimmeLoops &compute(const Function *fcn);
};

}


#endif


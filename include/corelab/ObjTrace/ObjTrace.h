#ifndef LLVM_CORELAB_OBJTRACE_H
#define LLVM_CORELAB_OBJTRACE_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

//#include "corelab/Profilers/campMeta.h"
//#include "corelab/Profilers/ContextTreeBuilder.h"
//#include "corelab/AliasAnalysis/LoopTraverse.hpp"
#include <fstream>
#include <map>

#ifndef DEBUG_TYPE
  #define DEBUG_TYPE "objtrace"
#endif


namespace corelab {
  using namespace llvm;
  using namespace std;

  typedef uint16_t InstrID;
  typedef uint64_t FullID;

  class ObjTrace : public ModulePass {
    public:
      bool runOnModule(Module& M);

      virtual void getAnalysisUsage(AnalysisUsage &AU) const {
        AU.addRequired< Namer >();
        AU.setPreservesAll();
      }

      const char *getPassName() const { return "ObjTrace"; }

      static char ID;
      ObjTrace() : ModulePass(ID) {}

    private:
      Module *module;

      Constant *objTraceInitialize;
      Constant *objTraceFinalize;

      Constant *objTraceLoadInstr;
      Constant *objTraceStoreInstr;

      Constant *objTraceMalloc;
      Constant *objTraceCalloc;
      Constant *objTraceRealloc;
      Constant *objTraceFree;

      void setFunctions(Module &M);
      void setIniFini(Module &M);

      void hookMallocFree();
      void makeMetadata(Instruction* Instruction, uint64_t id); // From Metadata/Namer
  }; // class
} // namespace

#endif

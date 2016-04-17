#ifndef LLVM_CORELAB_DEVICE_GRAPH_H
#define LLVM_CORELAB_DEVICE_GRAPH_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  struct DeviceGraphTableEntry {
    std::string dev1;
    std::string dev2;
    std::string protocol;
  };

  class DeviceGraph : public ModulePass {
    public:
      static char ID;

      DeviceGraph () : ModulePass (ID) {}

      void getAnalysisUsage (AnalysisUsage &AU) const;
      const char* getPassName () const { return "Device Graph"; }

      bool runOnModule (Module& M);
      //Function *makeLoopFunction(Loop *loop, DominatorTree &dt);
	    void buildDeviceGraph();
      void insertEdge(std::vector<struct DeviceGraphTableEntry> &DGTable, 
                      std::string dev1, 
                      std::string dev2,
                      std::string protocol);
  };
}
#endif

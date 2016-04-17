/***
 * DeviceGraph.cpp
 *
 * 
 * 
 * Written By Bongjun
 * */

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Esperanto/EspInit.h"
#include "corelab/Esperanto/DeviceGraph.h"
#include "corelab/Esperanto/StringUtils.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <string.h>
using namespace corelab;
using namespace std;

namespace corelab {

  char DeviceGraph::ID = 0;
  static RegisterPass<DeviceGraph> X("device-graph", "construct a graph of devices based on PTable", false, false);

  void DeviceGraph::getAnalysisUsage(AnalysisUsage& AU) const {
    AU.addRequired< EspInitializer >();
    AU.setPreservesAll();
  }

  bool DeviceGraph::runOnModule(Module& M)
  {
    //LLVMContext &Context = getGlobalContext();
    //buildMetadataTable();
    //buildDriverTable();
    //buildProtocolTable();
    //buildFunctionTable(M);
    buildDeviceGraph();
    return false;
  }

  void DeviceGraph::buildDeviceGraph() {
    printf("\n@@@@@ BONG @@@@ : \n");
    
    EspInitializer& database = getAnalysis< EspInitializer >();
    std::map<StringRef, ProtocolTableEntry> PTable = database.PTable.getProtocolTable();
		printf("ptable size = %d\n",(int)PTable.size());	
    std::vector<struct DeviceGraphTableEntry> DeviceGraphTable;

    std::map<StringRef, ProtocolTableEntry>::iterator it;
    std::map<StringRef, ProtocolTableEntry>::iterator it_inner;
    std::map<std::string, struct ProtocolInfo>::iterator it2;
    std::map<std::string, struct ProtocolInfo>::iterator it2_inner;
std::map<std::string, struct ProtocolInfo>::iterator it3;

		printf("\n\n\n");
    
		for (it = PTable.begin(); it != PTable.end(); ++it) {
      std::map<std::string, struct ProtocolInfo> hostProtocols = (it->second).getHostProtocols();
std::map<std::string, struct ProtocolInfo> hostProtocol = (it->second).getHostProtocols();
      printf(" && PTable first %s\n", (it->first).str().c_str());
      for (it2 = hostProtocols.begin(); it2 != hostProtocols.end(); ++it2) {
        printf("it2 hostProtocol %s\n", (it2->first).c_str());
        for (it_inner = PTable.begin(); it_inner != PTable.end(); ++it_inner) {
          if (it == it_inner) continue;
          std::map<std::string, struct ProtocolInfo> clntProtocols = (it_inner->second).getClntProtocols();
          for (it2_inner = clntProtocols.begin(); it2_inner != clntProtocols.end(); ++it2_inner) {
            if (it2->first == it2_inner->first) {
              // match !!
              printf(" && MATCH && \n %s's %s == %s's %s\n", (it->first).str().c_str()
                                                           , (it2->first).c_str()
                                                           , (it_inner->first).str().c_str()
                                                           , (it2_inner->first).c_str());
              insertEdge(DeviceGraphTable, it->first, it_inner->first, it2->first);
            } // if
          } // for it2_inner
        } // for it_inner
      } // for it2
    } // for it
    
    std::vector<struct DeviceGraphTableEntry>::iterator iter;
    for (iter = DeviceGraphTable.begin(); iter != DeviceGraphTable.end(); ++iter) {
      printf ("Entry : [%s, %s, %s]\n", (iter->dev1).c_str(), (iter->dev2).c_str(), (iter->protocol).c_str());
    }
    printf("\n@@@@@ BONG @@@@\n");

  } // function buildDeviceGraph

  void DeviceGraph::insertEdge(std::vector<struct DeviceGraphTableEntry> &DGTable, 
                               std::string dev1, 
                               std::string dev2, 
                               std::string protocol) {
    std::vector<struct DeviceGraphTableEntry>::iterator it;
    bool alreadyExistPair = false;
    for (it = DGTable.begin(); it != DGTable.end(); ++it) {
      if ((it->dev1 == dev1 && it->dev2 == dev2)
          || (it->dev1 == dev2 && it->dev2 == dev1)) {
        alreadyExistPair = true;
      } 
    }
    if (!alreadyExistPair) {
      struct DeviceGraphTableEntry DGTE = {dev1, dev2, protocol};
      DGTable.push_back(DGTE);
    }  
  }

} // class DeviceGraph

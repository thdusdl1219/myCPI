#include <iostream>
#include <set>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/CodeExtractor.h"
#include "corelab/Esperanto/RegionToFunction.h"

namespace corelab {
  using namespace llvm;
  using namespace std;

  static RegisterPass<RegionToFunction> X("region2function", "Region to Function", false, false);
  char RegionToFunction::ID = 0;

  void RegionToFunction::getAnalysisUsage (AnalysisUsage &AU) const {
    //AU.addRequired< LoopInfo > ();
    // AU.addRequired<IndVarSimplify>();
    AU.addRequired< LoopInfoWrapperPass>();
    AU.addRequired< DominatorTreeWrapperPass >();
    AU.setPreservesAll();
  }

  bool RegionToFunction::runOnModule (Module& M) {
    map< Loop* , StringRef > regions; 

    Function *mainFcn = M.getFunction("main");
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*mainFcn).getLoopInfo();
    DominatorTree& dt = getAnalysis<DominatorTreeWrapperPass>(*mainFcn).getDomTree();
    // LoopInfo &LI = getAnalysis<LoopInfo>(*mainFcn);

    for (LoopInfo::iterator li = LI.begin(); li != LI.end(); li++) {
      BasicBlock *header = (*li)->getHeader();
      for(BasicBlock::iterator bi = header->begin(); bi != header->end(); bi ++){
        if(isa<PHINode>(bi) && bi->getName().find("espRegion") != StringRef::npos)
          regions[*li] = bi->getName();
          //regions.insert(*li);
					//printf("this region is espRegion : %d\n",(int)regions.size());
      }
    }

    for(map< Loop*, StringRef >::iterator li = regions.begin(); li != regions.end(); li++){
      Function* newFunc = makeLoopFunction(li->first, dt);
			//printf("insert new function\n");
      newFunc->setName(StringRef("_") + li->second.split('.').first);
    }
/*
    for(set< Loop* >::iterator li = regions.begin(); li != regions.end(); li++){
			(*li)->dump();
      Function* newFunc = makeLoopFunction(*li, dt);

      newFunc->setName(Twine("espRegion"));
    }
*/
    return false;

  }

  Function* RegionToFunction::makeLoopFunction(Loop *loop, DominatorTree &dt) {
		//SmallVectorImpl<BasicBlock*> bvec(0);
		std::vector<BasicBlock*> all = loop->getBlocks();
		//loop->getExitingBlocks(bvec);
		//for(int i=0;i<bvec.size();i++){
		//	all.push_back(bvec[i]);
		//}
		return CodeExtractor(all, &dt, false).extractCodeRegion();
    //return CodeExtractor(dt, *loop, false).extractCodeRegion();
  }    
} 


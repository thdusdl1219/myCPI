#ifndef LLVM_CORELAB_SPLIT_EDGE_H
#define LLVM_CORELAB_SPLIT_EDGE_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/IR/Dominators.h"

namespace corelab
{
  using namespace llvm;

  /// Split an edge from the basic block 'from' to the basic block 'to'
  /// by inserting a basic block in between them.  Update
  /// terminators and phi instructions appropriately.
  /// Return the new basic block.
  /// Also, update dominator trees and dominance frontier.
  BasicBlock *split(BasicBlock *from, BasicBlock *to, DominatorTree &dt, DominanceFrontier &df, const char *prefix = 0) __attribute__ ((deprecated));

  /// Split an edge from the basic block 'from' to the basic block 'to'
  /// by inserting a basic block in between them.  Update
  /// terminators and phi instructions appropriately.
  /// Return the new basic block.
  /// Also, update dominator tree.
  BasicBlock *split(BasicBlock *from, BasicBlock *to, DominatorTree &dt, const char *prefix = 0) __attribute__ ((deprecated));


  /// Split an edge from the basic block 'from' to the basic block 'to'
  /// by inserting a basic block in between them.  Update
  /// terminators and phi instructions appropriately.
  /// Return the new basic block.
  BasicBlock *split(BasicBlock *from, BasicBlock *to, const char *prefix = 0) __attribute__ ((deprecated));

  /// Split an out edge from the basic block 'from'
  /// by inserting a basic block in between them.  Update
  /// terminators and phi instructions appropriately.
  /// Return the new basic block.
  BasicBlock *split(BasicBlock *from, unsigned succno, const char *prefix = 0);


}


#endif //LLVM_CORELAB_SPLIT_EDGE_H

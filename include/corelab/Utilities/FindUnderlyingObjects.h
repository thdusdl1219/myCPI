#ifndef FIND_UNDERLYING_OBJECTS_H
#define FIND_UNDERLYING_OBJECTS_H

#include "llvm/IR/Instructions.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Module.h"

#include <set>

namespace corelab {
  typedef llvm::DenseSet<const llvm::Value *> ObjectSet;
  /// TODO: replace this with GetUnderlyingObjects().
  void findUnderlyingObjects(llvm::Module* M, const llvm::Value *value, ObjectSet &values);

  typedef std::set<const llvm::Value *> UO;
  /// Like the previous, but handles PHI and SELECT, and uses
  /// a more appropriate data structure.
  void GetUnderlyingObjects(llvm::Module* M, const llvm::Value *ptr, UO &uo);

  /// Optionally, collect those objects found before/after a PHI
  /// node into separate collections.
  void GetUnderlyingObjects(llvm::Module* M, const llvm::Value *ptr, UO &beforePHI, UO &afterPHI, bool isAfterPHI=false);
}

#endif /* FIND_UNDERLYING_OBJECTS_H */


#ifndef CAST_UTIL_H
#define CAST_UTIL_H

#include "llvm/IR/Instruction.h"

#include "corelab/Utilities/InstInsertPt.h"

namespace corelab {

  llvm::Value *castToInt64Ty(llvm::Value *value,
                             corelab::InstInsertPt &out);

  llvm::Value *castFromInt64Ty(llvm::Type *ty, llvm::Value *value,
                               corelab::InstInsertPt &out);
}

#endif /* CAST_UTIL_H */

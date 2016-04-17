#include "llvm/IR/Constants.h"

namespace corelab {
  void replaceGlobalWith(llvm::GlobalVariable *oldGlobal,
                         llvm::GlobalVariable *newGlobal);
}

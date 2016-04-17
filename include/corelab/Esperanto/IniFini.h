#ifndef CORELAB_UTILITIES_INIFINI_H
#define CORELAB_UTILITIES_INIFINI_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"

#include "corelab/Esperanto/GlobalCtors.h"

using namespace llvm;

namespace corelab {
	Function* getOrInsertConstructor(Module& M);
	Function* getOrInsertDestructor(Module& M);
}


#endif

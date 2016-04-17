#ifndef LLVM_CORELAB_GEP_AND_LOAD_H
#define LLVM_CORELAB_GEP_AND_LOAD_H

#include "corelab/Utilities/InstInsertPt.h"

namespace corelab
{
using namespace llvm;

void storeIntoStructure(InstInsertPt &where, Value *valueToStore, Value *pointerToStructure, unsigned fieldOffset);
Value *loadFromStructure(InstInsertPt &where, Value *pointerToStructure, unsigned fieldOffset);

}


#endif


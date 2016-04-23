#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/DerivedTypes.h"

#include "corelab/UVA/StandAlonePartitioner.h"
      
#include <vector>

using namespace corelab;

static Function *getCalledFunction_aux(Instruction* indCall); // From AliasAnalysis/IndirectCallAnal.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall);

char StandAlonePartitioner::ID = 0;
static RegisterPass<StandAlonePartitioner> X("stand-alone-partitioner", "UVA stand-alone partioning.", false, false);

void StandAlonePartitioner::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

// command line argument
static cl::opt<string> ClientName("client_name", 
    cl::desc("Specify part"), 
    cl::value_desc("global initializer in charge"));

void StandAlonePartitioner::setFunctions(Module &M){
	LLVMContext &Context = M.getContext();
  
  VoidFunc = M.getOrInsertFunction(
      "void_func",
      Type::getVoidTy(Context),
      (Type*)0);
}
bool StandAlonePartitioner::runOnModule(Module& M) {
  setFunctions(M);

  std::vector<Instruction*> listToErase;
  CallInst *ci;
  for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function* F = &*fi;
    if (F->isDeclaration())
      continue;
    for (inst_iterator I = inst_begin(F); I != inst_end(F); ++I) {
      Instruction *instruction = &*I;
      if(isa<CallInst>(instruction)) {
        ci = dyn_cast<CallInst>(instruction);
        ci->dump();
        Function *callee = getCalledFunction_aux(instruction);
        if(!callee){
          const Value *calledVal = getCalledValueOfIndCall(instruction);
          if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
            callee = const_cast<Function *>(tarFun);
          }
        }
        if (!callee) continue;
        //if (callee->isDeclaration() == false) continue;
        if (callee->getName().find(ClientName) != std::string::npos) {
          printf("This callee (%s) belongs to (%s)\n", F->getName().data(), ClientName.data());
        } else if (callee->getName().find("device") != std::string::npos) {
          ci->setCalledFunction(VoidFunc);
        }
      }
    }
  }
  
  return true;
}

//TODO:: where to put this useful function?
//BONGJUN:: From CAMP/ContextTreeBuilder.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall){
  if(const CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledValue();
  }
  else if(const InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledValue();
  }
  else
    assert(0 && "WTF??");
}

//TODO:: where to put this useful function?
//BONGJUN:: From AliasAnalysis/IndirectCallAnal.cpp
static Function *getCalledFunction_aux(Instruction* indCall){
  if(CallInst *callInst = dyn_cast<CallInst>(indCall)){
    return callInst->getCalledFunction();
  }
  else if(InvokeInst *invokeInst = dyn_cast<InvokeInst>(indCall)){
    return invokeInst->getCalledFunction();
  }
  else
    assert(0 && "WTF??");
}

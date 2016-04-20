#include "corelab/UVA/Serverso.h"
#include "corelab/Utilities/GlobalCtors.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

using namespace corelab;

char UVAServer::ID = 0;
static RegisterPass<UVAServer> X("uva-server", "UVA server-side(x86).", false, false);

void UVAServer::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool UVAServer::runOnModule(Module& M) {
  setFunctions(M);


  setIniFini(M);
  return true;

}

void UVAServer::setFunctions(Module& M) {
  LLVMContext &Context = M.getContext();
 
  UVAServerInit = M.getOrInsertFunction(
      "UVAServerInitialize",
      Type::getVoidTy(Context),
      (Type*)0);

  UVAServerFinal = M.getOrInsertFunction(
      "UVAServerFinalize",
      Type::getVoidTy(Context),
      (Type*)0);
}

void UVAServer::setIniFini(Module& M) {
  LLVMContext &Context = M.getContext();
  std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
  
  FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

  Function *initForCtr = Function::Create(
      voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M);
  BasicBlock *entry = BasicBlock::Create(Context, "entry", initForCtr);
	BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 

  CallInst::Create(UVAServerInit, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	ReturnInst::Create(Context, 0, initBB);
	callBeforeMain(initForCtr);
	
	/* finalize */
	Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
	actuals.resize(0);
	CallInst::Create(UVAServerFinal, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);

}

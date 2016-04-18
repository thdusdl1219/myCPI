#include "corelab/UVA/Clientso.h"
#include "corelab/Utilities/GlobalCtors.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"

#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/HoistVariables.h"

#include <vector>

#define GLOBAL_DEBUG

using namespace corelab;

char UVAClient::ID = 0;
static RegisterPass<UVAClient> X("uva-client", "UVA client-side.", false, false);

void UVAClient::getAnalysisUsage(AnalysisUsage &AU) const {
  //AU.addRequired< Namer >();
  //AU.addRequired< LoadNamer >();
  AU.setPreservesAll();
}

bool UVAClient::runOnModule(Module& M) {
  setFunctions(M);

  // Filter out gloabl vars using ptr and move up to global vars.
  hoist = new HoistVariables(M);
  hoist->getGlobalVariableList(M);
  //hoist->createGlobalVariableFunctions(M.getDataLayout());

  hoist->distinguishGlobalVariables();

  setIniFini(M);
  return true;

}

void UVAClient::setFunctions(Module& M) {
  LLVMContext &Context = getGlobalContext();
 
  UVAClientInit = M.getOrInsertFunction(
      "UVAClientInitialize",
      Type::getVoidTy(Context),
      (Type*)0);

  UVAClientFinal = M.getOrInsertFunction(
      "UVAClientFinalize",
      Type::getVoidTy(Context),
      (Type*)0);
  
}

void UVAClient::setIniFini(Module& M) {
  LLVMContext &Context = getGlobalContext();
  std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
  
  FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

  Function *initForCtr = Function::Create(
      voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M);
  BasicBlock *entry = BasicBlock::Create(Context, "entry", initForCtr);
	BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 

  CallInst::Create(UVAClientInit, actuals, "", entry); 
	BranchInst::Create(initBB, entry); 
	Instruction *termIni = ReturnInst::Create(Context, 0, initBB);

  hoist->deployGlobalVariable(M, termIni, M.getDataLayout());
  hoist->hoistGlobalVariable(M, termIni, M.getDataLayout());
  hoist->initializeGlobalVariable(M, termIni, M.getDataLayout());
	callBeforeMain(initForCtr);
	
	/* finalize */
	Function *finiForDtr = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
	BasicBlock *finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
	BasicBlock *fini = BasicBlock::Create(Context, "fini", finiForDtr);
	actuals.resize(0);
	CallInst::Create(UVAClientFinal, actuals, "", fini);
	BranchInst::Create(fini, finiBB);
	ReturnInst::Create(Context, 0, fini);
	callAfterMain(finiForDtr);

}

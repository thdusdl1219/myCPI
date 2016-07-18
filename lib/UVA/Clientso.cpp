#include "corelab/UVA/Clientso.h"
#include "corelab/Utilities/GlobalCtors.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"

#include "corelab/Utilities/InstInsertPt.h"
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

// command line argument
static cl::opt<string> GVInitializer("gv_initializer", 
    cl::desc("Specify Global variable initializer (1: gv_initializer, 0: not)"), 
      cl::value_desc("global initializer in charge"));

bool UVAClient::runOnModule(Module& M) {
  setFunctions(M);

  // Filter out gloabl vars using ptr and move up to global vars.
  //hoist = new HoistVariables(M);
  //hoist->getGlobalVariableList(M);
  //hoist->createGlobalVariableFunctions(M.getDataLayout());

  //hoist->distinguishGlobalVariables();

  if(M.getFunction("__constructor__") != NULL){
    printf("ctor exists\n");
    modifyIniFini(M);
  } else {
    printf("ctor don't exists\n");
    setIniFini(M);
  }
  return true;

}

void UVAClient::setFunctions(Module& M) {
  LLVMContext &Context = M.getContext();
 
  UVAClientInit = M.getOrInsertFunction(
      "UVAClientInitialize",
      Type::getVoidTy(Context),
      Type::getInt32Ty(Context),
      (Type*)0);

  UVAClientFinal = M.getOrInsertFunction(
      "UVAClientFinalize",
      Type::getVoidTy(Context),
      (Type*)0);
  
}

/* XXX: this is for UVA-only module XXX*/
void UVAClient::setIniFini(Module& M) {
  LLVMContext &Context = M.getContext();
  std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
  
  FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

  Function *initForCtr = Function::Create(
      voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M);
  BasicBlock *entry = BasicBlock::Create(Context, "entry", initForCtr);
	BasicBlock *initBB = BasicBlock::Create(Context, "init", initForCtr); 

  std::vector<Value *> vecArgs(0);
  vecArgs.resize(1);
  if (strcmp(GVInitializer.data(),"1") == 0)
    vecArgs.push_back(ConstantInt::get(Type::getInt32Ty(Context), 1));
  else
    vecArgs.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0)); 
  CallInst::Create(UVAClientInit, vecArgs, "", entry); 
	BranchInst::Create(initBB, entry); 
	//Instruction *termIni = ReturnInst::Create(Context, 0, initBB);
	ReturnInst::Create(Context, 0, initBB);

  //hoist->deployGlobalVariable(M, termIni, M.getDataLayout());
  //hoist->hoistGlobalVariable(M, termIni, M.getDataLayout());
  //hoist->initializeGlobalVariable(M, termIni, M.getDataLayout());
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

/* XXX: this is for UVA in Esperanto XXX */
void UVAClient::modifyIniFini(Module &M) {
  LLVMContext &Context = M.getContext();
  std::vector<Type*> formals(0);
	std::vector<Value*> actuals(0);
  
  FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false); 

  //Function *ctor = M.getFunction("__constructor__");
  Function *dtor = M.getFunction("__destructor__");
  //assert(ctor != NULL && "wrong");
  //assert(dtor != NULL && "wrong");
  //BasicBlock *bbOfCtor = &(ctor->front());
  BasicBlock *bbOfDtor = &(dtor->front());

  /*Instruction *targetCallInst;
  InstInsertPt out;
  bool isExistEariler = false;
  for(inst_iterator I = inst_begin(ctor); I != inst_end(ctor); I++) {
    if(isa<CallInst>(&*I)) {
      CallInst *tarFun = dyn_cast<CallInst>(&*I);
      Function *callee = tarFun->getCalledFunction();
      if(callee->getName() == "EspInit") {
        targetCallInst = &*I;
        out = InstInsertPt::After(targetCallInst);
        isExistEariler = true;
        break;
      } else if(callee->getName().find("__decl_const_global_range__") != std::string::npos) {
        printf("UVAClient::modifyIniFini: fnDeclCRange exists!\n");
        targetCallInst = &*I;
        out = InstInsertPt::Before(targetCallInst);
        isExistEariler = true;
        break;
      }
    }
  }
  std::vector<Value *> vecArgs(0);
  vecArgs.resize(0);
  vecArgs.resize(1);
  if (strcmp(GVInitializer.data(),"1") == 0) {
    vecArgs[0] = (ConstantInt::get(Type::getInt32Ty(Context), 1));
  } else {
    vecArgs[0] = (ConstantInt::get(Type::getInt32Ty(Context), 0)); 
  }

  if (isExistEariler) {
    out << CallInst::Create(UVAClientInit, vecArgs, "");
  } else {
    CallInst::Create(UVAClientInit, vecArgs, "", bbOfCtor->getFirstNonPHI());
  }
*/
  if (bbOfDtor->getFirstNonPHI() != NULL) {
    CallInst::Create(UVAClientFinal, actuals, "", bbOfDtor->getFirstNonPHI());
  } else {
    CallInst::Create(UVAClientFinal, actuals, "");
  }
}

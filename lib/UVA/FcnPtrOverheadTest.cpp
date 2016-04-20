/***
 *
 * Server.cpp : Code partitioner for Server.
 *
 *
 * **/

#include "llvm/IR/LLVMContext.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/Utilities/Casting.h"
#include "corelab/Metadata/Metadata.h"
#include "corelab/Metadata/LoadNamer.h"
#include "corelab/UVA/FcnPtrOverheadTest.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

//#define GLOBAL_DEBUG
//#define OFFLOADED_FUNCTION_ONLY

using namespace corelab;
using namespace std;

char FcnPtrOverheadTest::ID = 0;
static RegisterPass<FcnPtrOverheadTest> X("fcnptr-ohdtest", "function pointer translation overhead test", false, false);

void FcnPtrOverheadTest::getAnalysisUsage(AnalysisUsage &AU) const {
	//AU.addRequired< DataLayout >();
	AU.addRequired< LoadNamer >();
	AU.setPreservesAll();
}

bool FcnPtrOverheadTest::runOnModule(Module& M) {
	LoadNamer& loadNamer = getAnalysis< LoadNamer >();
	//const DataLayout& dataLayout = getAnalysis< DataLayout >().getDataLayout();
  const DataLayout& dataLayout = M.getDataLayout();

	FunctionPointerTranslater* fcnptr = new FunctionPointerTranslater(M);
	// FIXME: install translator only for targets!!
	fcnptr->installTranslator(M, loadNamer, dataLayout);
	fcnptr->installBackTranslator(M, loadNamer, dataLayout);
	
	std::vector<Type*> formals(0);
	FunctionType *voidFcnVoidType = FunctionType::get(Type::getVoidTy(M.getContext ()), formals, false);
	Function *fnCtor = Function::Create( 
			voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M); 
	BasicBlock *blkCtor = BasicBlock::Create(M.getContext (), "ctor", fnCtor); 
	Instruction *instTerm = ReturnInst::Create (M.getContext (), 0, blkCtor);
	callBeforeMain(fnCtor, 0);
	
	Function *fnMain = M.getFunction ("main");
	CallInst::Create (fnCtor, "", fnMain->begin()->getFirstNonPHI());

	installRegistFcnPtr (M, instTerm, loadNamer, dataLayout);

	return false;
}

void FcnPtrOverheadTest::installRegistFcnPtr (Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = M.getContext();

	Constant *cnstRegistFnpt = M.getOrInsertFunction (
		"offloadServerRegistSelfFunctionPointer",
		Type::getVoidTy (Context),
		Type::getInt32Ty (Context),
		Type::getInt8PtrTy (Context),
		NULL);

	Constant *cnstEndRegistFnpt = M.getOrInsertFunction (
		"offloadServerEndRegistSelfFunctionPointer",
		Type::getVoidTy (Context),
		NULL);

	std::vector<Value*> actuals(0);
	actuals.resize(2);

	for (Module::iterator ifn = M.begin(); ifn != M.end (); ifn++) {
		Function *fn = &*ifn;

		if (fn->isDeclaration()) continue;
		uint32_t idFn = loadNamer.getFunctionId(*fn);
		if (idFn == 0) continue;

		InstInsertPt out = InstInsertPt::Before(I);
		Value* valTemp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		Value* valFnpt = Casting::castTo(fn, valTemp, out, &dataLayout);
		Value* valID = ConstantInt::get(Type::getInt32Ty(Context), idFn);
		actuals[0] = valID;
		actuals[1] = valFnpt;
		CallInst::Create(cnstRegistFnpt, actuals, "", I);
	}

	// end of the producing
	CallInst::Create(cnstEndRegistFnpt, "", I);

	return;
}

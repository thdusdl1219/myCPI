/***
 *
 * IniFini.cpp
 *
 * This module sets initializer and finalizer (a.k.a. constructor & destructor)
 * with the function name __constroctor__, __destructor__ respectively.
 * If there is already target function, just return it.
 *
 */

#include "llvm/ADT/SmallVector.h"
#include "corelab/Esperanto/IniFini.h"

using namespace llvm;

namespace corelab {
	
	// getOrInsertConstructor(Module& M)
	//
	// Initializer has two basic blocks; "entry" and "init"
	// This function is called before "main";
	Function* getOrInsertConstructor(Module& M) {
		/* initialize */
		Function* initForCtr = M.getFunction("__constructor__");
		if (initForCtr) return initForCtr;

		LLVMContext& Context = getGlobalContext();
		std::vector<Type*> formals(0);
		FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);
		
		initForCtr = Function::Create( 
				voidFcnVoidType, GlobalValue::InternalLinkage, "__constructor__", &M); 
		BasicBlock* entry = BasicBlock::Create(Context,"entry", initForCtr); 
		BasicBlock* initBB = BasicBlock::Create(Context, "init", initForCtr); 
		BranchInst::Create(initBB, entry); 
		ReturnInst::Create(Context, 0, initBB);
		callBeforeMain(initForCtr, 0);					
		return initForCtr;
	}
	
	// getOrInsertDestructor(Module& M)
	//
	// Finalizer has two basic blocks; "entry" and "init"
	// This function is called after "main";
	Function* getOrInsertDestructor(Module& M) {
		/* finalize */
		Function* finiForDtr = M.getFunction("__destructor__");
		if (finiForDtr) return finiForDtr;
		
		LLVMContext& Context = getGlobalContext();
		std::vector<Type*> formals(0);
		FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);

		finiForDtr = Function::Create(
				voidFcnVoidType, GlobalValue::InternalLinkage, "__destructor__",&M);
		BasicBlock* finiBB = BasicBlock::Create(Context, "entry", finiForDtr);
		BasicBlock* fini = BasicBlock::Create(Context, "fini", finiForDtr);
		BranchInst::Create(fini, finiBB);
		ReturnInst::Create(Context, 0, fini);
		callAfterMain(finiForDtr);
		return finiForDtr;
	}
}

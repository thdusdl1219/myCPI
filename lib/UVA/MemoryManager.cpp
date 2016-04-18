/***
 *
 * MemoryManager.cpp
 *
 * It hooks all the memory function calls and change them into 
 * offload_xxx memory managing function. In this file, it has
 * two module pass for 64bit and 32bit platform both.
 *
 * **/

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopInfo.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/UVA/MemoryManager.h"

#include <string>
#include <cstdlib>
#include <cstdio>

using namespace corelab;

// Utils
static bool isUseOfGetElementPtrInst(LoadInst *ld);
static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);
static Function *getCalledFunction_aux(Instruction* indCall); // From AliasAnalysis/IndirectCallAnal.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall);

static void installLoadStoreHandler(Module &M, Constant *Load, Constant *Store, bool is32);

//char MemoryManagerX86::ID = 0;
char MemoryManagerX64::ID = 0;
char MemoryManagerX64S::ID = 0;
char MemoryManagerArm::ID = 0;
//static RegisterPass<MemoryManagerX86> XA("mm-64", "substitute memory allocation for 64bit platform", false, false);
static RegisterPass<MemoryManagerX64> XB("mm-64", "substitute memory allocation for 64bit platform(Client)", false, false);
static RegisterPass<MemoryManagerX64S> XBS("mm-64S", "substitute memory allocation for 64bit platform(Server)", false, false);
static RegisterPass<MemoryManagerArm> XA("mm-32-arm", "substitute memory allocation for 32bit ARM platform(Client)", false, false);
//void MemoryManagerX86::getAnalysisUsage(AnalysisUsage &AU) const {
//	AU.setPreservesAll();
//}
void MemoryManagerX64::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
}

void MemoryManagerX64S::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
}

void MemoryManagerArm::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
}
//void MemoryManagerX86::setFunctions(Module &M) {
//	LLVMContext &Context = getGlobalContext();
//	voidTy = Type::getVoidTy(Context);
//	ptrTy = Type::getInt8PtrTy(Context);
//	intTy = Type::getInt64Ty(Context);
//	
//	Malloc = M.getOrInsertFunction(
//			"offload_malloc",
//			ptrTy,
//			intTy,
//			(Type*)0);
//
//	Calloc = M.getOrInsertFunction(
//			"offload_calloc",
//			ptrTy,
//			intTy,
//			intTy,
//			(Type*)0);
//
//	Realloc = M.getOrInsertFunction(
//			"offload_realloc",
//			ptrTy,
//			ptrTy,
//			intTy,
//			(Type*)0);
//
//	Free = M.getOrInsertFunction(
//			"offload_free",
//			voidTy,
//			ptrTy,
//			(Type*)0);
//	
//	return;
//}

void MemoryManagerX64::setFunctions(Module &M) {
	LLVMContext &Context = getGlobalContext();
	voidTy = Type::getVoidTy(Context);
	ptrTy = Type::getInt8PtrTy(Context);
	intTy = Type::getInt64Ty(Context);
	
	Malloc = M.getOrInsertFunction(
			"uva_malloc",
			ptrTy,
			intTy,
			(Type*)0);

	Calloc = M.getOrInsertFunction(
			"uva_calloc",
			ptrTy,
			intTy,
			intTy,
			(Type*)0);

	Realloc = M.getOrInsertFunction(
			"uva_realloc",
			ptrTy,
			ptrTy,
			intTy,
			(Type*)0);

	Free = M.getOrInsertFunction(
			"uva_free",
			voidTy,
			ptrTy,
			(Type*)0);

	Strdup = M.getOrInsertFunction(
			"uva_strdup",
			ptrTy,
			ptrTy,
			(Type*)0);

	Mmap = M.getOrInsertFunction(		// XXX 'mmap' is quite dangerous for this system.
			"uva_mmap",
			ptrTy,
			ptrTy,
			intTy,
			intTy,
			intTy,
			intTy,
			intTy,
			(Type*)0);

	// XXX 'munmap' skipped

  Load = M.getOrInsertFunction(
      "uva_load",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Length */
      (Type*)0);

  Store = M.getOrInsertFunction(
      "uva_store",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Length */
      Type::getInt64PtrTy(Context),
      (Type*)0);

  /* _Znmw : 64 bit */
  New64 = M.getOrInsertFunction(
      "uva_new64",
      Type::getInt8PtrTy(Context),
      Type::getInt64Ty(Context),
      (Type*)0);

  /* _Znmj : 32 bit */
  New32 = M.getOrInsertFunction(
    "uva_new32",
    Type::getInt8PtrTy(Context),
    Type::getInt32Ty(Context),
    (Type*)0);

	return;
}

void MemoryManagerX64S::setFunctions(Module &M) {
	LLVMContext &Context = getGlobalContext();
	voidTy = Type::getVoidTy(Context);
	ptrTy = Type::getInt8PtrTy(Context);
	intTy = Type::getInt64Ty(Context);
	
	Malloc = M.getOrInsertFunction(
			"uva_server_malloc",
			ptrTy,
			intTy,
			(Type*)0);

	Calloc = M.getOrInsertFunction(
			"uva_server_calloc",
			ptrTy,
			intTy,
			intTy,
			(Type*)0);

	Realloc = M.getOrInsertFunction(
			"uva_server_realloc",
			ptrTy,
			ptrTy,
			intTy,
			(Type*)0);

	Free = M.getOrInsertFunction(
			"uva_free",
			voidTy,
			ptrTy,
			(Type*)0);

	Strdup = M.getOrInsertFunction(
			"uva_server_strdup",
			ptrTy,
			ptrTy,
			(Type*)0);

	Mmap = M.getOrInsertFunction(		// XXX 'mmap' is quite dangerous for this system.
			"uva_server_mmap",
			ptrTy,
			ptrTy,
			intTy,
			intTy,
			intTy,
			intTy,
			intTy,
			(Type*)0);

	// XXX 'munmap' skipped

  Load = M.getOrInsertFunction(
      "uva_server_load",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Length */
      (Type*)0);

  Store = M.getOrInsertFunction(
      "uva_server_store",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Address */
      Type::getInt64Ty(Context), /* Length */
      Type::getInt64PtrTy(Context),
      (Type*)0);

  /* _Znmw : 64 bit */
  New64 = M.getOrInsertFunction(
      "uva_server_new64",
      Type::getInt8PtrTy(Context),
      Type::getInt64Ty(Context),
      (Type*)0);

  /* _Znmj : 32 bit */
  New32 = M.getOrInsertFunction(
    "uva_server_new32",
    Type::getInt8PtrTy(Context),
    Type::getInt32Ty(Context),
    (Type*)0);

	return;
}

void MemoryManagerArm::setFunctions(Module &M) {
	LLVMContext &Context = getGlobalContext();
	voidTy = Type::getVoidTy(Context);
	ptrTy = Type::getInt8PtrTy(Context);
	intTy = Type::getInt32Ty(Context);
	
	Malloc = M.getOrInsertFunction(
			"uva_malloc",
			ptrTy,
			intTy,
			(Type*)0);

	Calloc = M.getOrInsertFunction(
			"uva_calloc",
			ptrTy,
			intTy,
			intTy,
			(Type*)0);

	Realloc = M.getOrInsertFunction(
			"uva_realloc",
			ptrTy,
			ptrTy,
			intTy,
			(Type*)0);

	Free = M.getOrInsertFunction(
			"uva_free",
			voidTy,
			ptrTy,
			(Type*)0);

	Strdup = M.getOrInsertFunction(
			"uva_strdup",
			ptrTy,
			ptrTy,
			(Type*)0);

	Mmap = M.getOrInsertFunction(		// XXX 'mmap' is quite dangerous for this system.
			"uva_mmap",
			ptrTy,
			ptrTy,
			intTy,
			intTy,
			intTy,
			intTy,
			intTy,
			(Type*)0);

	// XXX 'munmap' skipped

  Load = M.getOrInsertFunction(
      "uva_load",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt32Ty(Context), /* Address */
      Type::getInt32Ty(Context), /* Length */
      (Type*)0);

  Store = M.getOrInsertFunction(
      "uva_store",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt32Ty(Context), /* Address */
      Type::getInt32Ty(Context), /* Length */
      Type::getInt32PtrTy(Context), /* XXX: Not sure */
      (Type*)0);

  // XXX: New64 need on ARM ??? XXX
  /* _Znmw : 64 bit 
  New64 = M.getOrInsertFunction(
      "uva_new64",
      Type::getInt8PtrTy(Context),
      Type::getInt64Ty(Context),
      (Type*)0);
*/
  /* _Znmj : 32 bit */
  New32 = M.getOrInsertFunction(
    "uva_new32",
    Type::getInt8PtrTy(Context),
    Type::getInt32Ty(Context),
    (Type*)0);

	return;
}
//bool MemoryManagerX86::runOnModule(Module& M) {
//	setFunctions(M);
//	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
//		Function* F = &*fi;
//
//		if (F->isDeclaration())
//			continue;
//		runOnFunction(F);
//	}
//	
//	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
//		Function* F = &*fi;
//		// case of use
//		StringRef ma("malloc");
//		StringRef fr("free");
//		StringRef ca("calloc");
//		StringRef re("realloc");
//
//		if(F->getName().equals(ma)) {
//			FunctionType* ftype = F->getFunctionType();
//			Constant* Ma = M.getOrInsertFunction(
//					"offload_malloc", ftype);
//			F->replaceAllUsesWith(Ma);
//		}
//		if(F->getName().equals(fr)) {
//			FunctionType* ftype = F->getFunctionType();
//			Constant* Fr = M.getOrInsertFunction(
//					"offload_free", ftype);
//			F->replaceAllUsesWith(Fr);
//		}
//		if(F->getName().equals(ca)) {
//			FunctionType* ftype = F->getFunctionType();
//			Constant* Ca = M.getOrInsertFunction(
//					"offload_calloc", ftype);
//			F->replaceAllUsesWith(Ca);
//		}
//		if(F->getName().equals(re)) {
//			FunctionType* ftype = F->getFunctionType();
//			Constant* Re = M.getOrInsertFunction(
//					"offload_realloc", ftype);
//			F->replaceAllUsesWith(Re);
//		}
//	}
//	return false;
//}

bool MemoryManagerX64::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;
		runOnFunction(F);
	}
  installLoadStoreHandler(M, Load, Store, false);


#if 0
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		// case of use
		StringRef ma("malloc");
		StringRef fr("free");
		StringRef ca("calloc");
		StringRef re("realloc");
		StringRef du("strdup"); 

		if(F->getName().equals(ma) || isCppNewOperator(F)) {
			FunctionType* ftype = F->getFunctionType();
			Constant* Ma = M.getOrInsertFunction(
					"offload_malloc", ftype);
			F->replaceAllUsesWith(Ma);
		}
		else if(F->getName().equals(fr) || isCppDeleteOperator(F)) {
			FunctionType* ftype = F->getFunctionType();
			Constant* Fr = M.getOrInsertFunction(
					"offload_free", ftype);
			F->replaceAllUsesWith(Fr);
		}
		else if(F->getName().equals(ca)) {
			FunctionType* ftype = F->getFunctionType();
			Constant* Ca = M.getOrInsertFunction(
					"offload_calloc", ftype);
			F->replaceAllUsesWith(Ca);
		}
		else if(F->getName().equals(re)) {
			FunctionType* ftype = F->getFunctionType();
			Constant* Re = M.getOrInsertFunction(
					"offload_realloc", ftype);
			F->replaceAllUsesWith(Re);
		}
		else if(F->getName().equals(du)) {
			FunctionType* ftype = F->getFunctionType();
			Constant* Du = M.getOrInsertFunction(
					"offload_strdup", ftype);
			F->replaceAllUsesWith(Du);
		}
	}
#endif
		
	return false;
}

bool MemoryManagerX64S::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;
		runOnFunction(F);
	}
  installLoadStoreHandler(M, Load, Store, false);
  return false;
}

bool MemoryManagerArm::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration())
			continue;
		runOnFunction(F);
	}
  installLoadStoreHandler(M, Load, Store, true);
  return false;
}
//bool MemoryManagerX86::runOnFunction(Function *F) {
//	StringRef ma("malloc");
//	StringRef fr("free");
//	StringRef ca("calloc");
//	StringRef re("realloc");
//
//	// Set up the call Instructions for each instructions
//	for (Function::iterator fi = F->begin(), fe = F->end(); fi != fe; ++fi) {
//		for (BasicBlock::iterator ii = fi->begin(), ie = fi->end(); ii != ie; ++ii) {
//			Instruction *instruction = &*ii;
//	
//			// swap the memory function to mine version
//			if (instruction->getOpcode() == Instruction::Call) {
//				CallInst *cd = (CallInst*)instruction;
//
//				Function* f = cd->getCalledFunction();
//				if (f != NULL) {
//					if (f->getName().equals(ma))
//						cd->setCalledFunction(Malloc);
//					else if (f->getName().equals(fr))
//						cd->setCalledFunction(Free);	
//					else if (f->getName().equals(ca))
//						cd->setCalledFunction(Calloc);	
//					else if (f->getName().equals(re))
//						cd->setCalledFunction(Realloc);	
//				}
//			}
//		}
//	}
//	return false;
//}

bool MemoryManagerX64::runOnFunction(Function *F) {

		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			Instruction *instruction = &*I;
			bool wasBitCasted = false;
			Type *ty;
			IRBuilder<> Builder(instruction);
			if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
				Function *callee = getCalledFunction_aux(instruction);
				if(!callee){
					const Value *calledVal = getCalledValueOfIndCall(instruction);
					if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
						wasBitCasted = true;
						ty = calledVal->getType();
						callee = const_cast<Function *>(tarFun);
					}
				}
				if(!callee) continue;
				if(callee->isDeclaration() == false) continue;

				if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value * changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else
					assert(0&&"ERROR!!");
			}
		}
  return false;

/*	StringRef ma("malloc");
	StringRef fr("free");
	StringRef ca("calloc");
	StringRef re("realloc");
	StringRef du("strdup");
	StringRef mm("mmap");

	// Set up the call Instructions for each instructions
	for (Function::iterator fi = F->begin(), fe = F->end(); fi != fe; ++fi) {
		for (BasicBlock::iterator ii = fi->begin(), ie = fi->end(); ii != ie; ++ii) {
			Instruction *instruction = &*ii;
	
			// swap the memory function to mine version
			if (instruction->getOpcode() == Instruction::Call) {
				CallInst *cd = (CallInst*)instruction;

				Function* f = cd->getCalledFunction();
				if (f != NULL) {
					if (f->getName().equals(ma) || isCppNewOperator(f))
						cd->setCalledFunction(Malloc);
					else if (f->getName().equals(fr) || isCppDeleteOperator(f))
						cd->setCalledFunction(Free);	
					else if (f->getName().equals(ca))
						cd->setCalledFunction(Calloc);	
					else if (f->getName().equals(re))
						cd->setCalledFunction(Realloc);	
					else if (f->getName().equals(du))
						cd->setCalledFunction(Strdup);	
					else if (f->getName().equals(mm))
						cd->setCalledFunction(Mmap);
				}
			}
		}
	}
	return false; */
}

bool MemoryManagerX64S::runOnFunction(Function *F) {

		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			Instruction *instruction = &*I;
			bool wasBitCasted = false;
			Type *ty;
			IRBuilder<> Builder(instruction);
			if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
				Function *callee = getCalledFunction_aux(instruction);
				if(!callee){
					const Value *calledVal = getCalledValueOfIndCall(instruction);
					if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
						wasBitCasted = true;
						ty = calledVal->getType();
						callee = const_cast<Function *>(tarFun);
					}
				}
				if(!callee) continue;
				if(callee->isDeclaration() == false) continue;

				if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value * changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else
					assert(0&&"ERROR!!");
			}
		}
  return false;
}

bool MemoryManagerArm::runOnFunction(Function *F) {

		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			Instruction *instruction = &*I;
			bool wasBitCasted = false;
			Type *ty;
			IRBuilder<> Builder(instruction);
			if(isa<InvokeInst>(instruction) || isa<CallInst>(instruction)){
				Function *callee = getCalledFunction_aux(instruction);
				if(!callee){
					const Value *calledVal = getCalledValueOfIndCall(instruction);
					if(const Function *tarFun = dyn_cast<Function>(calledVal->stripPointerCasts())){
						wasBitCasted = true;
						ty = calledVal->getType();
						callee = const_cast<Function *>(tarFun);
					}
				}
				if(!callee) continue;
				if(callee->isDeclaration() == false) continue;

				if(CallInst *callInst = dyn_cast<CallInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value * changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          /*else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }*/
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else if(InvokeInst *callInst = dyn_cast<InvokeInst>(instruction)){
					if(callee->getName() == "malloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
					}
					else if(callee->getName() == "calloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Calloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Calloc);
						}
					}
					else if(callee->getName() == "realloc"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Realloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Realloc);
						}
					}
					else if(callee->getName() == "free"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Free, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Free);
						}
					}
          /*else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }*/
          else if(callee->getName() == "Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(New32);
						}
          }
				}
				else
					assert(0&&"ERROR!!");
			}
		}
  return false;
}

/* installLoadStoreHandler: for both X64, Arm */
static void installLoadStoreHandler(Module &M, Constant *Load, Constant *Store, bool is32) {
  for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function &F = *fi;
    LLVMContext &Context = getGlobalContext();
    const DataLayout &dataLayout = M.getDataLayout();
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      // For each load instructions
      if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
        if(isUseOfGetElementPtrInst(ld) == false){
          args.resize (2);
          Value *addr = ld->getPointerOperand();
          Value *temp;
          if(!is32){
            temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
          } else {
            temp = ConstantInt::get(Type::getInt32Ty(Context), 0);
          }
          InstInsertPt out = InstInsertPt::Before(ld);
          addr = castTo(addr, temp, out, &dataLayout);

          //InstrID instrId = Namer::getInstrId(instruction);
          //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
          //FullID fullId = Namer::getFullId(instruction);
          //Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);
          //for debug
          //errs()<<"<"<<instrId<<"> "<<*ld<<"\n";

          //DEBUG(errs()<< "load instruction id %" << fullId << "\n");

          // DEBUG(errs() << "load inst's type : ");
          //printf("ld getType : ");
          //ld->getType()->dump();
          //DEBUG(errs() << "load inst's 1st operand type : ");
          //printf("ld operand 0 type : ");
          
          //ld->getOperand(0)->getType()->dump();
          //addr->getType()->dump();
          //ld->getType()->dump();

          //printf("load getTypeAllocSize : %lu\n", dataLayout.getTypeAllocSize(ld->getType()));
          //printf("load length : %d\n", ld->getType()->getScalarSizeInBits());
          // LoadLength: means what type of value want to get. type is represented by bit length.
          uint64_t loadTypeSize = dataLayout.getTypeAllocSize(ld->getType());
          Value *loadTypeSize_;
          if(!is32) {
            loadTypeSize_ = ConstantInt::get(Type::getInt64Ty(Context), loadTypeSize);
          } else {
            loadTypeSize_ = ConstantInt::get(Type::getInt32Ty(Context), loadTypeSize);
          }
          args[0] = addr;
          //args[1] = fullId_;
          args[1] = loadTypeSize_; 
          CallInst::Create(Load, args, "", ld);
        }
      }
      // For each store instructions
      else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
        args.resize (3);
        Value *addr = st->getPointerOperand();
        Value *temp;
        if(!is32){
          temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
        } else {
          temp = ConstantInt::get(Type::getInt32Ty(Context), 0);
        }
        InstInsertPt out = InstInsertPt::Before(st);
        addr = castTo(addr, temp, out, &dataLayout);

        //InstrID instrId = Namer::getInstrId(instruction);
        //Value *instructionId = ConstantInt::get(Type::getInt16Ty(Context), instrId);
        //FullID fullId = Namer::getFullId(instruction);
        //Value *fullId_ = ConstantInt::get(Type::getInt64Ty(Context), fullId);

        //DEBUG(errs()<< "store instruction id %" << fullId << "\n");

        //DEBUG(errs() << "store inst's type : ");
        //st->getType()->dump();
        //DEBUG(errs() << "store inst's addr operand type : ");
        //st->getOperand(0)->dump();
        //printf("st operand 0 type : ");
        //addr->getType()->dump();
        //st->getValueOperand()->getType()->dump();
        
        unsigned int storeValueTypeSize = dataLayout.getTypeAllocSize(st->getValueOperand()->getType());
        Value *storeValueTypeSize_;
        if(!is32) {
          storeValueTypeSize_ = ConstantInt::get(Type::getInt64Ty(Context), storeValueTypeSize);
        } else {
          storeValueTypeSize_ = ConstantInt::get(Type::getInt32Ty(Context), storeValueTypeSize);
        }
        Value *valueOperand = st->getValueOperand();
        if(!is32){
          temp = ConstantPointerNull::get(Type::getInt64PtrTy(Context));
        } else {
          temp = ConstantPointerNull::get(Type::getInt32PtrTy(Context));
        }
        valueOperand = castTo(valueOperand, temp, out, &dataLayout);
        
        args[0] = addr;
        //args[1] = fullId_;
        args[1] = storeValueTypeSize_;
        args[2] = valueOperand;
        CallInst::Create(Store, args, "", st);
      }
    } // for
  } // for

}

bool MemoryManagerX64::isCppDeleteOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Zdl" || prefix == "_Zda") return true;

	return false;
}

bool MemoryManagerX64::isCppNewOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Znw" || prefix == "_Zna") return true;

	return false;
}



bool MemoryManagerX64S::isCppNewOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Znw" || prefix == "_Zna") return true;

	return false;
}

bool MemoryManagerX64S::isCppDeleteOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Zdl" || prefix == "_Zda") return true;

	return false;
}


bool MemoryManagerArm::isCppNewOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Znw" || prefix == "_Zna") return true;

	return false;
}

bool MemoryManagerArm::isCppDeleteOperator(Function* F) {
	string name = F->getName().str();
	if (name.length() > 6) return false;
	if (name.length() < 4) return false;

	string prefix = name.substr (0, 4);
	if (prefix == "_Zdl" || prefix == "_Zda") return true;

	return false;
}
//Utility
static bool isUseOfGetElementPtrInst(LoadInst *ld){
  // is only Used by GetElementPtrInst ?
  return std::all_of(ld->user_begin(), ld->user_end(), [](User *user){return isa<GetElementPtrInst>(user);});
}

static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl)
{
  LLVMContext &Context = getGlobalContext();
  const size_t fromSize = dl->getTypeSizeInBits( from->getType() );
  const size_t toSize = dl->getTypeSizeInBits( to->getType() );

  // First make it an integer
  if( ! from->getType()->isIntegerTy() ) {
    // cast to integer of same size of bits
    Type *integer = IntegerType::get(Context, fromSize);
    Instruction *cast;
    if( from->getType()->getTypeID() == Type::PointerTyID )
      cast = new PtrToIntInst(from, integer);
    else {
      cast = new BitCastInst(from, integer);
    }
    out << cast;
    from = cast;
  }

  // Next, make it have the same size
  if( fromSize < toSize ) {
    Type *integer = IntegerType::get(Context, toSize);
    Instruction *cast = new ZExtInst(from, integer);
    out << cast;
    from = cast;
  } else if ( fromSize > toSize ) {
    Type *integer = IntegerType::get(Context, toSize);
    Instruction *cast = new TruncInst(from, integer);
    out << cast;
    from = cast;
  }

  // possibly bitcast it to the approriate type
  if( to->getType() != from->getType() ) {
    Instruction *cast;
    if( to->getType()->getTypeID() == Type::PointerTyID )
      cast = new IntToPtrInst(from, to->getType() );
    else {
      cast = new BitCastInst(from, to->getType() );
    }

    out << cast;
    from = cast;
  }

  return from;
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

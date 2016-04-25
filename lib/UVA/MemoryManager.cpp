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
#include "llvm/IR/Constants.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/LoopInfo.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/GlobalCtors.h"
#include "corelab/UVA/MemoryManager.h"

#include <string>
#include <cstdlib>
#include <cstdio>

//#define DEBUG_MM

using namespace corelab;

// Utils
static bool isUseOfGetElementPtrInst(LoadInst *ld);
static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);
static Function *getCalledFunction_aux(Instruction* indCall); // From AliasAnalysis/IndirectCallAnal.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall);
static const User *isGEP(const Value *V);

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
	LLVMContext &Context = M.getContext();
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
			Type::getInt32Ty(Context),
			Type::getInt32Ty(Context),
			Type::getInt32Ty(Context),
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
	LLVMContext &Context = M.getContext();
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
	LLVMContext &Context = M.getContext();
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
		runOnFunction(F, false);
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
		runOnFunction(F, true);
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
bool MemoryManagerX64::runOnFunction(Function *F, bool is32) {

		for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
			Instruction *instruction = &*I;
      /*if(isGEP(instruction)) {
        printf("************************gep\n");
        instruction->dump();
      } else {
        for (auto Op = I->op_begin(); Op != I->op_end(); ++Op) {
          if(isGEP(*Op)) {
            printf("@@@@@@@@@@@@@@@@@@@@@@@gep\n");
            instruction->dump();
          }
        }
      }*/
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
          /* @SPECIAL CASE: [converting "mmap" to "uva_mmap"]
           *  This mmap transformation is a special case.  When a client want
           *  to initialize global variables in fixed address space
           *  (0x15000000~some point), he "mmap" first. But server have to know
           *  and initialize too. That's why we change "mmap" to "uva_mmap".
           *  TLDR; global variable initialization in initial time should be
           *  synchronized with server.
           **/
          else if(callee->getName() == "mmap"){
            int intAddr = 0;
            if(ConstantExpr *constexp = dyn_cast<ConstantExpr>(instruction->getOperand(0))) {
              if(constexp->isCast()) {
                Instruction *inttoptrInst = constexp->getAsInstruction();
                Value *addr = inttoptrInst->getOperand(0);
                if(ConstantInt *CI = dyn_cast<ConstantInt>(addr)) {
                  if(CI->getBitWidth() <= 64) {
                    intAddr = CI->getZExtValue();
                  }
                  if (0x15000000 <= intAddr && intAddr <= 0x16000000) {
                    printf("mmap addr (%p) is in fixed global address interval\n", (void*)intAddr);
                    if(wasBitCasted){
                      Value *changeTo = Builder.CreateBitCast(Mmap, ty);
                      callInst->setCalledFunction(changeTo);
                    } else {
                      callInst->setCalledFunction(Mmap);
                    }
                    inttoptrInst->insertBefore(instruction);
                    instruction->setOperand(0,inttoptrInst);
                  }
                }
              }
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

bool MemoryManagerArm::runOnFunction(Function *F, bool is32) {

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
          else if(callee->getName() == "mmap"){
            Value *addr = instruction->getOperand(0);
            int intAddr = 0;
            if(ConstantInt *CI = dyn_cast<ConstantInt>(addr)) {
              if(CI->getBitWidth() <= 64) {
                intAddr = CI->getZExtValue();
              }
                printf("mmap addr (%p)\n", (void*)intAddr);
              if (0x15000000 < intAddr && intAddr <= 0x16000000) {
                printf("mmap addr (%p) is in fixed global address interval\n", (void*)addr);
                if(wasBitCasted){
                  Value *changeTo = Builder.CreateBitCast(Mmap, ty);
                  callInst->setCalledFunction(changeTo);
                } else {
                  callInst->setCalledFunction(Mmap);
                }
              }
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
    LLVMContext &Context = M.getContext();
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

          // LoadLength: means what type of value want to get. type is represented by bit length.
          uint64_t loadTypeSize = dataLayout.getTypeAllocSize(ld->getType());
          Value *loadTypeSize_;
          if(!is32) {
            loadTypeSize_ = ConstantInt::get(Type::getInt64Ty(Context), loadTypeSize);
          } else {
            loadTypeSize_ = ConstantInt::get(Type::getInt32Ty(Context), loadTypeSize);
          }
          args[0] = addr;
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
        
        Value *valueOperand = st->getValueOperand();
        unsigned int storeValueTypeSize = dataLayout.getTypeAllocSize(valueOperand->getType());
        Value *storeValueTypeSize_;
        if(!is32) {
          storeValueTypeSize_ = ConstantInt::get(Type::getInt64Ty(Context), storeValueTypeSize);
        } else {
          storeValueTypeSize_ = ConstantInt::get(Type::getInt32Ty(Context), storeValueTypeSize);
        }
        if(!is32){
          temp = ConstantPointerNull::get(Type::getInt64PtrTy(Context));
        } else {
          temp = ConstantPointerNull::get(Type::getInt32PtrTy(Context));
        }

        if (ArrayType *tyArr = dyn_cast<ArrayType>(valueOperand->getType())){
#ifdef DEBUG_MM
          printf("mm: valueOperand is Array type!, and type size is %d\n", storeValueTypeSize);
#endif
          vector<Value*> vecGepIdx;
          vecGepIdx.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
#ifdef DEBUG_MM
          tyArr->dump();
          st->getPointerOperand()->dump();
#endif          
          GetElementPtrInst *gepInst = GetElementPtrInst::Create(tyArr, st->getPointerOperand(), vecGepIdx, "ty.arr.ptr", st); 
#ifdef DEBUG_MM
          unsigned int arrElemTypeSize = dataLayout.getTypeAllocSize(valueOperand->getType()->getArrayElementType());
          printf("mm: store's operand (size:%d) (elem:%d) (total:%d):\n", storeValueTypeSize, arrElemTypeSize, storeValueTypeSize*arrElemTypeSize);
          
          valueOperand->dump();
          printf("mm: type id is %d", valueOperand->getType()->getTypeID());
          printf(" , ptr Ty ID is %d\n",valueOperand->getType()->getPointerTo()->getTypeID());
#endif    
          Value *arrOperand = castTo(gepInst, temp, out, &dataLayout);

          args[0] = addr;
          args[1] = storeValueTypeSize_;
          args[2] = arrOperand;
          CallInst::Create(Store, args, "", st);
        } else {
          valueOperand = castTo(valueOperand, temp, out, &dataLayout);

          args[0] = addr;
          args[1] = storeValueTypeSize_;
          args[2] = valueOperand;
          CallInst::Create(Store, args, "", st);
        }
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
  LLVMContext &Context = from->getContext();
  const size_t fromSize = dl->getTypeSizeInBits( from->getType() );
  const size_t toSize = dl->getTypeSizeInBits( to->getType() );

#ifdef DEBUG_MM
  printf("mm: castTo: fromSize (%d), toSize (%d)\n", fromSize, toSize);
#endif
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
#ifdef DEBUG_MM
  else if (from->getType()->isIntegerTy()){
    printf("mm: castTo: from is IntegerTy\n");
  }
#endif
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
#ifdef DEBUG_MM
  printf("mm: castTo: AFTER making same size\n");
#endif
  // possibly bitcast it to the approriate type
  if( to->getType() != from->getType() ) {
    Instruction *cast;
    if( to->getType()->getTypeID() == Type::PointerTyID )
      cast = new IntToPtrInst(from, to->getType() );
    else {
#ifdef DEBUG_MM
      printf("mm: castTo: to's typeID is NOT PointerTyID\n");
      from->getType()->dump();
      to->getType()->dump();
#endif
      cast = new BitCastInst(from, to->getType() );
    }

    out << cast;
    from = cast;
  }
#ifdef DEBUG_MM
  printf("mm: castTo: end of castTo()\n\n");
#endif
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

// This is from old version of llvm/lib/Analysis/BasicAliasAnalysis.cpp
static const User *isGEP(const Value *V) {
  if (isa<GetElementPtrInst>(V) ||
      (isa<ConstantExpr>(V) &&
       cast<ConstantExpr>(V)->getOpcode() == Instruction::GetElementPtr))
    return cast<User>(V);
  return 0;
}
/*
static const User *isGEP_(const Value *V) {
  if (isa<GetElementPtrInst(V))
    return cast<User>(V);
  if (Operator *Op = dyn_cast<Operator>(V)) {
    if (Op->getOpcode() == Instruction::BitCast ||
        Op->getOpcode() == Instruction::AddrSpaceCast) {

    }
  }
}*/

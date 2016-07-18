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
#define HLRC

using namespace corelab;

// Utils
static bool isUseOfGetElementPtrInst(LoadInst *ld);
static Value* castTo(Value* from, Value* to, InstInsertPt &out, const DataLayout *dl);
static Function *getCalledFunction_aux(Instruction* indCall); // From AliasAnalysis/IndirectCallAnal.cpp
static const Value *getCalledValueOfIndCall(const Instruction* indCall);
static const User *isGEP(const Value *V);

static void filterStackAddrAccess(Module &M); 
static void installMemAccessHandler(Module &M, 
    Constant *Load, 
    Constant *Store, 
    Constant *Memset,
    Constant *Memcpy,
    Constant *Memmove, bool is32);

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
      Type::getInt64Ty(Context), /* Length */
      Type::getInt8PtrTy(Context), /* Address */
      (Type*)0);

  Store = M.getOrInsertFunction(
      "uva_store",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt64Ty(Context), /* Length */
      Type::getInt8PtrTy(Context), /* value */
      Type::getInt8PtrTy(Context), /* Address */
      (Type*)0);

  Memset = M.getOrInsertFunction(
      "uva_memset",
      Type::getInt8PtrTy(Context),
      Type::getInt8PtrTy(Context),
      Type::getInt32Ty(Context),
      Type::getInt64Ty(Context),
      (Type*)0);

  Memcpy = M.getOrInsertFunction(
      "uva_memcpy",
      Type::getInt8PtrTy(Context), 
      Type::getInt8PtrTy(Context),
      Type::getInt8PtrTy(Context),
      Type::getInt64Ty(Context), /* size_t */
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
      Type::getInt32Ty(Context), /* Length */
      Type::getInt8PtrTy(Context), /* Address */
      (Type*)0);

  Store = M.getOrInsertFunction(
      "uva_store",
      Type::getVoidTy(Context), /* Return type */
      Type::getInt32Ty(Context), /* Length */
      Type::getInt8PtrTy(Context), /* XXX: Not sure */
      Type::getInt8PtrTy(Context), /* Address */
      (Type*)0);
  
  Memset = M.getOrInsertFunction(
      "uva_memset",
      Type::getInt8PtrTy(Context),
      Type::getInt8PtrTy(Context),
      Type::getInt32Ty(Context),
      Type::getInt32Ty(Context),
      (Type*)0);

  Memcpy = M.getOrInsertFunction(
      "uva_memcpy",
      Type::getInt8PtrTy(Context), 
      Type::getInt8PtrTy(Context),
      Type::getInt8PtrTy(Context),
      Type::getInt32Ty(Context), /* size_t (32 in arm(?)) */
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

bool MemoryManagerX64::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration()) continue;
		runOnFunction(F, false);
	}
  filterStackAddrAccess(M);
  installMemAccessHandler(M, Load, Store, Memset, Memcpy, Memmove, false);
	return false;
}

bool MemoryManagerX64S::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration()) continue;
		runOnFunction(F);
	}
  //installLoadStoreHandler(M, Load, Store, false);
  return false;
}

bool MemoryManagerArm::runOnModule(Module& M) {
	setFunctions(M);
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function* F = &*fi;
		if (F->isDeclaration()) continue;
		runOnFunction(F, true);
	}
  filterStackAddrAccess(M);
  installMemAccessHandler(M, Load, Store, Memset, Memcpy, Memmove, true);
  return false;
}

bool MemoryManagerX64::runOnFunction(Function *F, bool is32) {

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
          else if(callee->getName() == "memcpy"){
            Module *M = F->getParent();
            std::vector<Value*> args(0);
            args.resize(3);

            Value *dest = instruction->getOperand(0);
            Value *src = instruction->getOperand(1);
            Value *num = instruction->getOperand(2);
            Value *temp;

            temp = ConstantPointerNull::get(Type::getInt8PtrTy(M->getContext()));
            //printf("adrspace: %d\n",temp->getType()->getIntegerBitWidth());
            InstInsertPt out = InstInsertPt::Before(instruction);
            dest = castTo(dest, temp, out, &(M->getDataLayout()));
            src = castTo(src, temp, out, &(M->getDataLayout()));

            uint64_t ci_num;
            if(ConstantInt *CI_num = dyn_cast<ConstantInt>(num)) {
              unsigned int bitwidth = CI_num->getBitWidth();
              ci_num = CI_num->getZExtValue();
              temp = ConstantInt::get(IntegerType::get(M->getContext(), bitwidth), ci_num);
            }
            num = castTo(num, temp, out, &(M->getDataLayout()));
            args[0] = dest; // void* (Int8Ptr)
            args[1] = src; // voud* (Int8Ptr)
            args[2] = num; // size_t (Int64Ty or Int32Ty)
            CallInst::Create(Memcpy, args, "", instruction);

          }
          else if(callee->getName() == "_Znwm"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "_Znam"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "_Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(New32, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
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
          else if(callee->getName() == "_Znam"){
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
          else if(callee->getName() == "_Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "_Znaj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
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
          else if(callee->getName() == "_Znwj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
          else if(callee->getName() == "_Znaj"){
						if(wasBitCasted){
							Value *changeTo = Builder.CreateBitCast(Malloc, ty);
							callInst->setCalledFunction(changeTo);
						} else {
							callInst->setCalledFunction(Malloc);
						}
          }
				}
				else
					assert(0&&"ERROR!!");
			}
		}
  return false;
}

static bool getIsStackValue(Value *V) {
  if(AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
    //AI->dump();
    //printf("return true!!\n");
    return true; 
  } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(V)) {
    //BCI->dump();
    return getIsStackValue(BCI->getOperand(0)); 
  } else if (IntToPtrInst *ITPI = dyn_cast<IntToPtrInst>(V)) {
    return getIsStackValue(ITPI->getOperand(0));
  } else if (PtrToIntInst *PTII = dyn_cast<PtrToIntInst>(V)) {
    //PTII->dump();
    return getIsStackValue(PTII->getOperand(0));
  } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(V)) {
    //GEPI->dump();
    //GEPI->getOperand(0)->dump();
    return getIsStackValue(GEPI->getOperand(0)); 
  } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
    //GV->dump();
    if (GV->isExternalLinkage(GV->getLinkage())){
      //printf("return true!!\n");
      return true;
    }
  /* XXX Esperanto-specific case XXX */
  } else if (CallInst *CI = dyn_cast<CallInst>(V)) {
    Function *F = CI->getCalledFunction();
    if(F == NULL) return false;
    if (strcmp(F->getName().data(), "consumeReturn") == 0) {
      //printf("return true!!\n");
      return true;
    }
  } else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
    //LI->dump();
    Function *F = LI->getFunction();
    for (auto I = inst_begin(F); I != inst_end(F); ++I) {
      if (StoreInst *SI = dyn_cast<StoreInst>(&*I)) {
        //SI->dump();
        if(strcmp(SI->getPointerOperand()->getName().data(),LI->getPointerOperand()->getName().data()) == 0) {
          //SI->dump();
          //SI->getOperand(0)->dump();
          return getIsStackValue(SI->getOperand(0));
        }
      }
    }
  } 
  return false;
}

std::vector<Instruction*> vecUVAInst;

static void filterStackAddrAccess(Module &M) {
  for (Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function &F = *fi;
    LLVMContext &Context = M.getContext();
    const DataLayout &dataLayout = M.getDataLayout();
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      // For each load instructions
      if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
        if(!getIsStackValue(ld->getPointerOperand())) {
          vecUVAInst.push_back((Instruction*)ld);
        }
      } else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
        if(!getIsStackValue(st->getPointerOperand())) {
          vecUVAInst.push_back((Instruction*)st);
        }
      } else if (MemSetInst *MSI = dyn_cast<MemSetInst>(instruction)) {
        //if(!getIsStackValue(MSI->getDest())) {
          vecUVAInst.push_back((Instruction*)MSI);
        //}
      } else if (MemCpyInst *MCI = dyn_cast<MemCpyInst>(instruction)) {
        //if(!getIsStackValue(MCI->getDest())) {
          vecUVAInst.push_back((Instruction*)MCI);
        //}
      }
    } 
  }
  printf("\n\n################## num of uva mem access : %d\n", vecUVAInst.size());
}
/* installMemAccessHandler: for both X64, Arm */
static void installMemAccessHandler(Module &M, 
    Constant *Load, 
    Constant *Store, 
    Constant *Memset, 
    Constant *Memcpy, 
    Constant *Memmove, bool is32) {
  int count = 0;
  for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
    Function &F = *fi;
    LLVMContext &Context = M.getContext();
    const DataLayout &dataLayout = M.getDataLayout();
    std::vector<Value*> args(0);
    if (F.isDeclaration()) continue;
    if (strcmp(F.getName().data(), "EsperantoExecFunction") == 0) continue;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I){
      Instruction *instruction = &*I;
      // For each load instructions
      if(LoadInst *ld = dyn_cast<LoadInst>(instruction)) {
        count++;
        //if(isUseOfGetElementPtrInst(ld) == false){
          //ld->dump();
          if (find(vecUVAInst.begin(), vecUVAInst.end(), (Instruction*)ld) == vecUVAInst.end()) continue;
          args.resize (2);
          Value *addr = ld->getPointerOperand();

          
          /*if(AllocaInst *AI = dyn_cast<AllocaInst>(addr)) {
            continue; 
          } else if (BitCastInst *BCI = dyn_cast<BitCastInst>(addr)) {
            continue; 
          } else if (IntToPtrInst *ITPI = dyn_cast<IntToPtrInst>(addr)) {
            continue;
          } else if (GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(addr)) {
            continue; 
          } else if (GlobalVariable *GV = dyn_cast<GlobalVariable>(addr)) {
            if (GV->isExternalLinkage(GV->getLinkage())) {
              //GV->dump();
              continue;
            } else {
              //GV->dump();
              count++;
            }
          } else {
            addr->dump();
            count++;
          }*/
          
          Value *temp;
          //if(!is32){
          //  temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
          //} else {
          //  temp = ConstantInt::get(Type::getInt32Ty(Context), 0);
          //}
          temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
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
          args[0] = loadTypeSize_; 
          args[1] = addr;
#ifndef HLRC
          CallInst::Create(Load, args, "", ld);
#endif
        //}
      }
      // For each store instructions
      else if (StoreInst *st = dyn_cast<StoreInst>(instruction)) {
        count++;
        //st->dump();
        if (find(vecUVAInst.begin(), vecUVAInst.end(), (Instruction*)st) == vecUVAInst.end()) continue;
        MDNode *metadata = st->getMetadata("nouva");
        if (metadata != NULL) continue;
        args.resize (3);
        Value *addr = st->getPointerOperand();
        Value *temp;
        /*if(!is32){
          temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
        } else {
          temp = ConstantInt::get(Type::getInt32Ty(Context), 0);
        }*/
          temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
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
        /*if(!is32){
          temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        } else {
          temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        }*/

        temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
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

          args[0] = storeValueTypeSize_;
          args[1] = arrOperand;
          args[2] = addr;
          CallInst::Create(Store, args, "", st);
        } else if (StructType *tyStruct = dyn_cast<StructType>(valueOperand->getType())) {
          printf("mm: valueOperand is Struct Type!, type size is %d\n", storeValueTypeSize);

          vector<Value*> vecGepIdx;
          vecGepIdx.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
#ifdef DEBUG_MM
          tyStruct->dump();
          st->getPointerOperand()->dump();
#endif          
          GetElementPtrInst *gepInst = GetElementPtrInst::Create(tyStruct, st->getPointerOperand(), vecGepIdx, "ty.struct.ptr", st); 
#ifdef DEBUG_MM
          //unsigned int elemTypeSize = dataLayout.getTypeAllocSize(tyStruct->getElementType());
          //printf("mm: store's operand (size:%d) (elem:%d) (total:%d):\n", storeValueTypeSize, elemTypeSize, storeValueTypeSize*elemTypeSize);
          
          valueOperand->dump();
          printf("mm: type id is %d", valueOperand->getType()->getTypeID());
          printf(" , ptr Ty ID is %d\n",valueOperand->getType()->getPointerTo()->getTypeID());
#endif    
          Value *structOperand = castTo(gepInst, temp, out, &dataLayout);

          args[0] = storeValueTypeSize_;
          args[1] = structOperand;
          args[2] = addr;
          CallInst::Create(Store, args, "", st);
        } else if (VectorType *tyVector = dyn_cast<VectorType>(valueOperand->getType())) {
          printf("mm: valueOperand is Vector Type!, type size is %d\n", storeValueTypeSize);

          vector<Value*> vecGepIdx;
          vecGepIdx.push_back(ConstantInt::get(Type::getInt32Ty(Context), 0));
#ifdef DEBUG_MM
          tyVector->dump();
          st->getPointerOperand()->dump();
#endif          
          GetElementPtrInst *gepInst = GetElementPtrInst::Create(tyVector, st->getPointerOperand(), vecGepIdx, "ty.vector.ptr", st); 
#ifdef DEBUG_MM
          //unsigned int elemTypeSize = dataLayout.getTypeAllocSize(tyStruct->getElementType());
          //printf("mm: store's operand (size:%d) (elem:%d) (total:%d):\n", storeValueTypeSize, elemTypeSize, storeValueTypeSize*elemTypeSize);
          
          valueOperand->dump();
          printf("mm: type id is %d", valueOperand->getType()->getTypeID());
          printf(" , ptr Ty ID is %d\n",valueOperand->getType()->getPointerTo()->getTypeID());
#endif    
          Value *vectorOperand = castTo(gepInst, temp, out, &dataLayout);

          args[0] = storeValueTypeSize_;
          args[1] = vectorOperand;
          args[2] = addr;
          CallInst::Create(Store, args, "", st);
        } else { /* TODO: may need to handle "vector" type operand */
          valueOperand = castTo(valueOperand, temp, out, &dataLayout);

          args[0] = storeValueTypeSize_;
          args[1] = valueOperand;
          args[2] = addr;
          CallInst::Create(Store, args, "", st);
        }
      } else if (MemSetInst *MSI = dyn_cast<MemSetInst>(instruction)) {
        count++;
        //MSI->dump();
        if (find(vecUVAInst.begin(), vecUVAInst.end(), (Instruction*)MSI) == vecUVAInst.end()) continue;
        args.resize(3);
        
        Value *addr = MSI->getDest();
        Value *value = MSI->getValue();
        Value *num = MSI->getLength();
        Value *temp;
        
        temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        //printf("adrspace: %d\n",temp->getType()->getIntegerBitWidth());
        InstInsertPt out = InstInsertPt::Before(MSI);
        addr = castTo(addr, temp, out, &dataLayout);
       
        uint64_t ci_value;
        if(ConstantInt *CI_value = dyn_cast<ConstantInt>(value)) {
          ci_value = CI_value->getZExtValue();
        } 
        Value *value_ = ConstantInt::get(Type::getInt32Ty(Context), ci_value);
        
        uint64_t ci_num;
        if(ConstantInt *CI_num = dyn_cast<ConstantInt>(num)) {
          unsigned int bitwidth = CI_num->getBitWidth();
          ci_num = CI_num->getZExtValue();
          temp = ConstantInt::get(IntegerType::get(Context, bitwidth), ci_num);
        }
        num = castTo(num, temp, out, &dataLayout);
        args[0] = addr; // void* (Int8Ptr)
        args[1] = value_; // int (Int32Ty)
        args[2] = num; // size_t (Int64Ty or Int32Ty)
        CallInst::Create(Memset, args, "", MSI);
      } else if (MemCpyInst *MCI = dyn_cast<MemCpyInst>(instruction)) {
        count++;
        //MCI->dump();
        if (find(vecUVAInst.begin(), vecUVAInst.end(), (Instruction*)MCI) == vecUVAInst.end()) continue;
        args.resize(3);
        
        Value *dest = MCI->getDest();
        Value *src = MCI->getSource();
        Value *num = MCI->getLength();
        Value *temp;
        
        temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        //printf("adrspace: %d\n",temp->getType()->getIntegerBitWidth());
        InstInsertPt out = InstInsertPt::Before(MCI);
        dest = castTo(dest, temp, out, &dataLayout);
        src = castTo(src, temp, out, &dataLayout);

        uint64_t ci_num;

        /*if(ConstantInt *CI_num = dyn_cast<ConstantInt>(num)) {
          unsigned int bitwidth = CI_num->getBitWidth();
          printf("bitwidth : %d\n", bitwidth);
          ci_num = CI_num->getZExtValue();
          temp = ConstantInt::get(IntegerType::get(Context, bitwidth), ci_num);
        } else {
          printf("num (3rd arg) is not constant int\n");
        }*/
        /*if(is32) {
          temp = ConstantInt::get(Type::getInt32Ty(Context), 0);
        } else {
          temp = ConstantInt::get(Type::getInt64Ty(Context), 0);
        }
        num = castTo(num, temp, out, &dataLayout);*/
        args[0] = dest; // void* (Int8Ptr)
        args[1] = src; // void* (Int8Ptr)
        args[2] = num; // size_t (Int64Ty or Int32Ty)
        CallInst::Create(Memcpy, args, "", MCI);
      } else if (MemMoveInst *MMI = dyn_cast<MemMoveInst>(instruction)) {
        printf("MemMoveInst! Unimplemented!!\n");
      }
    } // for
  } // for
  printf("\n\n$$$$$$$$$$$$$$$ original uva operation : %d\n\n", count);
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
    //printf("mm: castTo: from is IntegerTy\n");
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
  //printf("mm: castTo: AFTER making same size\n");
#endif
  // possibly bitcast it to the approriate type
  if( to->getType() != from->getType() ) {
    Instruction *cast;
    if( to->getType()->getTypeID() == Type::PointerTyID )
      cast = new IntToPtrInst(from, to->getType() );
    else {
#ifdef DEBUG_MM
      //printf("mm: castTo: to's typeID is NOT PointerTyID\n");
#endif
      cast = new BitCastInst(from, to->getType() );
    }

    out << cast;
    from = cast;
  }
#ifdef DEBUG_MM
  //printf("mm: castTo: end of castTo()\n\n");
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

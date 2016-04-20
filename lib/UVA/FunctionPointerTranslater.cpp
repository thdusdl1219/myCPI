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
#include "corelab/UVA/FunctionPointerTranslater.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <vector>

using namespace corelab;
using namespace std;

void FunctionPointerTranslater::setFunctions(Module& M) {
	LLVMContext& Context = M.getContext();
	ProduceFptr = M.getOrInsertFunction(
			"offloadClientProduceFunctionPointer",
			Type::getVoidTy(Context),
			Type::getInt8PtrTy(Context),
			Type::getInt32Ty(Context),
			(Type*)0);

	ConsumeFptr = M.getOrInsertFunction(
			"offloadServerConsumeFunctionPointer",
			Type::getVoidTy(Context),
			(Type*)0);

	FunctionList = M.getOrInsertFunction(
			"offloadServerFunctionList",
			Type::getVoidTy(Context),
			Type::getInt32Ty(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	Translate = M.getOrInsertFunction(
			"offloadServerTranslate",
			Type::getInt8PtrTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	TranslateBack = M.getOrInsertFunction(
			"offloadServerTranslateBack",
			Type::getInt8PtrTy(Context),
			Type::getInt8PtrTy(Context),
			(Type*)0);

	return;
}

FunctionPointerTranslater::FunctionPointerTranslater(Module& M) {
	init(M);
}

void FunctionPointerTranslater::init(Module& M) {
	setFunctions(M);
	return;
}

void FunctionPointerTranslater::installBackTranslator(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	typedef std::pair<User *, Function *> UserToFnPtPair;
	typedef std::map<Instruction *, std::vector<UserToFnPtPair> > BackTransMap;

	LLVMContext& Context = M.getContext();
	BackTransMap mapBackTrans;

	for (FI fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function *F = &*fi;
		if (F->isDeclaration()) continue;

		for (BI bi = F->begin(), be = F->end(); bi != be; ++bi) {
			BasicBlock *B = &*bi;

			for (II ii = B->begin(), ie = B->end(); ii != ie; ++ii) {
				Instruction *inst = &*ii;

				// FIXME: should take all address-taken cases into account
				if(StoreInst *instStore = dyn_cast<StoreInst> (inst)) {
					if (Function *fn = dyn_cast<Function> (instStore->getValueOperand ())) 
						mapBackTrans[inst].push_back (UserToFnPtPair (inst, fn));
					else if (ConstantExpr *cexp = dyn_cast<ConstantExpr> (instStore->getValueOperand ())) {
						if (cexp->getOpcode () == Instruction::BitCast) {
							Value *valCasted = cexp->op_begin()->get ();

							if (Function *fn = dyn_cast<Function> (valCasted))
								mapBackTrans[inst].push_back (UserToFnPtPair (cexp, fn));
						}
					}
				}
				if(CallInst *instCall = dyn_cast<CallInst> (inst)) {
					/* check whether the called function is external */
					Function *fnCalled = instCall->getCalledFunction ();

					if (!fnCalled) {		/* strip bitcast */
						ConstantExpr *cexpCalled = dyn_cast<ConstantExpr> (instCall->getCalledValue ());
						if (cexpCalled && cexpCalled->getOpcode () == Instruction::BitCast)
							fnCalled = dyn_cast<Function> (cexpCalled->op_begin()->get ());
					}

					/* FIXME: function pointers not handled properly 
					 * 	If a external function receives a function pointer
					 * 	and is possibly called through its address,
					 * 	it should be wrapped up, so that the wrapper function 
					 *	can translate the function pointer passed by argument,
					 * 	and feed it to the original external one. */

					/* for now, we simply don't install back-translator
					 * if the caller is explicitly external function.
					 * We need not to handle this special case,
					 * if every external functions receiving a function pointer as an argument
					 * are wrapped up as mentioned above */
					if (fnCalled && fnCalled->isDeclaration ()) continue;

					for (int i = 0; i < instCall->getNumArgOperands (); i++) {
						if (Function *fn = dyn_cast<Function> (instCall->getArgOperand (i))) {
							mapBackTrans[inst].push_back (UserToFnPtPair (inst, fn));
						}
						else if (ConstantExpr *cexp = dyn_cast<ConstantExpr> (instCall->getArgOperand (i))) {
							if (cexp->getOpcode () == Instruction::BitCast) {
								if (Function *fnOper = dyn_cast<Function> (cexp->op_begin()->get())) {
									mapBackTrans[inst].push_back (UserToFnPtPair (cexp, fnOper));
								}
							}
						}
					}
				}
				else if (SelectInst *instSelect = dyn_cast<SelectInst> (inst)) {
					if (instSelect->getTrueValue ()) {
						if (Function *fn = dyn_cast<Function> (instSelect->getTrueValue ()))
							mapBackTrans[inst].push_back (UserToFnPtPair (inst, fn));
					}
					if (instSelect->getFalseValue ()) {
						if (Function *fn = dyn_cast<Function> (instSelect->getFalseValue ()))
							mapBackTrans[inst].push_back (UserToFnPtPair (inst, fn));
					}
				}

			}
		}
	}

	std::vector<Value*> actuals(0);
	actuals.resize(1);

	for (BackTransMap::iterator imap = mapBackTrans.begin ();
			 imap != mapBackTrans.end (); ++imap) {
		Instruction *inst = imap->first;
		std::vector<UserToFnPtPair> &vecTPairs = imap->second;

		for (std::vector<UserToFnPtPair>::iterator itpair = vecTPairs.begin ();
				 itpair != vecTPairs.end (); ++itpair) {
			User *user = itpair->first;
			Function *fnSFnPt = itpair->second;
			Type *tyFnPt = fnSFnPt->getType ();

			InstInsertPt out = InstInsertPt::Before (inst);
			Constant *cnstTemp = ConstantPointerNull::get (Type::getInt8PtrTy (Context));
			Value *valCasted = Casting::castTo (fnSFnPt, cnstTemp, out, &dataLayout);

			actuals[0] = valCasted;
			Instruction *instTrans = CallInst::Create (TranslateBack, actuals, "", inst);
			Constant *cnstRTemp = Constant::getNullValue (tyFnPt);
			Value *valCFnPt = Casting::castTo (instTrans, cnstRTemp, out, &dataLayout);

			if (user == inst)
				inst->replaceUsesOfWith (fnSFnPt, valCFnPt);
			else if (ConstantExpr *cexp = dyn_cast<ConstantExpr> (user)) {
				if (cexp->getOpcode () == Instruction::BitCast) {
					Value *valCCFnPt = new BitCastInst (valCFnPt, cexp->getType(), "", inst);
					inst->replaceUsesOfWith (cexp, valCCFnPt);
				}
				else
					assert (0 && "unsupported constant type");
			}
			else 
				assert (dyn_cast<Constant> (user) && "user must be either instruction or constant");
		}
	}
	fprintf(stderr, "Info: Function Pointer Back-translation # - %d\n", mapBackTrans.size());
	
	return;
}	

void FunctionPointerTranslater::installTranslator(Module& M, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = M.getContext();
	std::vector<CallInst *> calls;
	std::vector<Instruction *> subs;

	for (Module::iterator ifn = M.begin(); ifn != M.end (); ++ifn) {
		Function *fn = &*ifn;
		if (fn->isDeclaration()) continue;

		for (Function::iterator iblk = fn->begin(); iblk != fn->end (); ++iblk) {
			BasicBlock *blk = &*iblk;

			for (BasicBlock::iterator iinst = blk->begin(); iinst != blk->end (); ++iinst) {
				Instruction *inst = &*iinst;

				if (CallInst *instCall = dyn_cast<CallInst> (inst)) {
					Value* valCalled = instCall->getCalledValue();

					// FIXME: other corner-cases must be handled
					if (Instruction *instCalled = dyn_cast<Instruction> (valCalled)) {
						calls.push_back (instCall);
						subs.push_back (instCalled);
					}
#if 0
						if (isa<LoadInst> (calledV)) {
							calls.push_back(instCall);
							subs.push_back((Instruction*)calledV);
						} else if (isa<BitCastInst>(calledV)) {
							calls.push_back(instCall);
							subs.push_back((Instruction*)calledV);
						} else {
							fprintf(stderr, "function pointer is from other instruction "
								 "rather than loadinst\n");
							fprintf(stderr, "  %s, %s, %s\n", instCall->getName().data(),
								 	calledV->getName().data(), 
									((Instruction*)calledV)->getOpcodeName());
						}
#endif
				}
			}
		}
	}

	std::vector<Value*> actuals(0);
/*
	std::vector<Type*> formals(0);
	FunctionType* voidFcnVoidType = FunctionType::get(Type::getVoidTy(Context), formals, false);
	Function* Test = Function::Create(voidFcnVoidType, GlobalValue::InternalLinkage, "TestForFptr", &M);
	BasicBlock* entry = BasicBlock::Create(Context, "entry", Test);
	ReturnInst::Create(Context, 0, entry);
	for (uint32_t i = 0; i < calls.size(); ++i) {
		CallInst* c = calls[i];
		CallInst::Create(Test, actuals, "", c);
	}
*/
	actuals.resize(1);
	for (uint32_t i = 0; i < calls.size(); ++i) {
		CallInst* c = calls[i];
		Instruction* s = subs[i];
		Type* fptrType = s->getType();

		InstInsertPt out = InstInsertPt::Before(c);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		Value* casted = Casting::castTo(s, temp, out, &dataLayout);
		actuals[0] = casted;
		Value* translated = CallInst::Create(Translate, actuals, "", c);
		Value* templ = Constant::getNullValue(fptrType);
		Value* replacedFptr = Casting::castTo(translated, templ, out, &dataLayout);
		c->replaceUsesOfWith(s, replacedFptr);
	}
	fprintf(stderr, "Info: Function Pointer Translation # - %d\n", calls.size());
	
	return;
}

void FunctionPointerTranslater::produceFunctionPointers(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = M.getContext();
	std::vector<Value*> actuals(0);
	actuals.resize(2);
	for (FI fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function *F = &*fi;
		if (F->isDeclaration()) continue;
		uint32_t functionId = loadNamer.getFunctionId(*F);
		if (functionId == 0) continue;

		InstInsertPt out = InstInsertPt::Before(I);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		Value* fp = Casting::castTo(F, temp, out, &dataLayout);
		Value* id = ConstantInt::get(Type::getInt32Ty(Context), functionId);
		actuals[0] = fp;
		actuals[1] = id;
		CallInst::Create(ProduceFptr, actuals, "", I);
	}

	// end of the producing
	Value* fpNull = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
	Value* idNull = ConstantInt::get(Type::getInt32Ty(Context), 0);
	actuals[0] = fpNull;
	actuals[1] = idNull;
	CallInst::Create(ProduceFptr, actuals, "", I);
	return;
}

void FunctionPointerTranslater::consumeFunctionPointers(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	std::vector<Value*> actuals(0);
	CallInst::Create(ConsumeFptr, actuals, "", I);
	return;
}

void FunctionPointerTranslater::installToAddrRegisters(Module& M, Instruction* I, LoadNamer& loadNamer, const DataLayout& dataLayout) {
	LLVMContext& Context = M.getContext();
	std::vector<Value*> actuals(0);

	// Get remained function list
	for(Module::iterator fi = M.begin(), fe = M.end(); fi != fe; ++fi) {
		Function *F = &*fi;
		if (F->isDeclaration()) continue;
		uint32_t functionId = loadNamer.getFunctionId(*F);

#if 0
		// FIXME: only for encoder_demo
		if (functionId == 0) {
			if (F->getName () == "main") {
				F->setName (string ("h264ref_main"));
				functionId = loadNamer.getFunctionId(*F);
				F->setName (string ("main"));
			}
		}
#endif

		if (functionId == 0) continue;
		
		InstInsertPt out = InstInsertPt::Before(I);
		Value* temp = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
		Value* fp = Casting::castTo(F, temp, out, &dataLayout);
		Value* id = ConstantInt::get(Type::getInt32Ty(Context), functionId);
		actuals.resize(2);
		actuals[0] = id;
		actuals[1] = fp;
		CallInst::Create(FunctionList, actuals, "", I);
	}
}
